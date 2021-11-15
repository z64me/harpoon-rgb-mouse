/*
 * cli.c <z64.me>
 *
 * a command line program for interfacing
 * with a Corsair Harpoon mouse
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "harpoon.h"

#define DPIMODE_COUNT 6

struct dpimode
{
	int precision;
	unsigned int color;
};

/* fatal error message */
static void die(const char *fmt, ...)
{
	va_list ap;
	
	if (!fmt)
		exit(EXIT_FAILURE);
		
	fprintf(stderr, "[!] ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	
	exit(EXIT_FAILURE);
}

static void showargs(void)
{
#define P(X) fprintf(stderr, X "\n")
	P("  -p, --polling   set the mouse's polling rate (in Hertz)");
	P("                  e.g. --polling 1000");
	P("  -d, --dpi       change color and precision of one DPI setting 0 - 5");
	P("                  --dpi index precision 0xHexColor");
	P("                  e.g. --dpi 4 1000 0xff0000");
	P("  -o, --only      tell mouse's DPI button to allow only the specified modes");
	P("                  e.g. --only 012345 (enables all modes)");
	P("  -s, --simple    lock mouse into one color and precision setting");
	P("                  e.g. --simple precision 0xHexColor");
#undef P
	exit(EXIT_FAILURE);
}

/* retrieve and validate color */
static unsigned int get_color_from_string(const char *str)
{
	unsigned int color;
	
	if (sscanf(str, "%x", &color) != 1
		|| color > 0xffffff
	)
		die("invalid color '%s'; hex value must be < 0xffffff", str);
	
	return color;
}

/* retrieve and validate precision */
static int get_precision_from_string(const char *str)
{
	int precision;
	int multiple = 250;
	int minval = 250;
	int maxval = 6000;
	
	/* retrieve and validate precision */
	if (sscanf(str, "%d", &precision) != 1
		|| precision < minval
		|| precision > maxval
		|| (precision % multiple)
	)
		die(
			"invalid precision '%s'; "
			"decimal value must be multiple of %d, between %d and %d"
			, str
			, multiple
			, minval
			, maxval
		);
	
	return precision;
}

int main(int argc, char *argv[])
{
	const char *errstr = 0;
	const char *only = 0;
	struct harpoon *hp = 0;
	struct dpimode dpimode[DPIMODE_COUNT] = {0};
	int polling = 0;
	int i;
	
	if (argc < 3)
		showargs();
	
	/* step through arguments */
	for (i = 1; i < argc; )
	{
#define ARGMATCH(ALIAS, NAME) (!strcasecmp(this, "-" ALIAS) \
	|| !strcasecmp(this, "--" NAME))
#define PARAM(X) argv[i + 1 + X]
		const char *this = argv[i];
		
		if (ARGMATCH("p", "polling"))
		{
			const char *pollingStr = PARAM(0);
			
			if (!pollingStr)
				die("arg %s not enough arguments", this);
			
			/* retrieve and validate polling rate */
			if (sscanf(pollingStr, "%d", &polling) != 1
				|| (polling != 1000
					&& polling != 500
					&& polling != 250
					&& polling != 125
				)
			)
				die("invalid polling rate; valid options: 1000, 500, 250, 125");
			
			/* skip argument and param(s) */
			i += 2;
		}
		else if (ARGMATCH("d", "dpi"))
		{
			const char *indexStr = PARAM(0);
			const char *precisionStr = PARAM(1);
			const char *colorStr = PARAM(2);
			struct dpimode *mode;
			int index;
			
			if (!indexStr || !precisionStr || !colorStr)
				die("arg %s not enough arguments", this);
			
			/* retrieve and validate index */
			if (sscanf(indexStr, "%d", &index) != 1
				|| index < 0
				|| index >= DPIMODE_COUNT
			)
				die(
					"invalid index '%s'; needs decimal value between 0 and %d"
					, indexStr
					, DPIMODE_COUNT - 1
				);
			
			/* set up DPI mode */
			mode = &dpimode[index];
			mode->precision = get_precision_from_string(precisionStr);
			mode->color = get_color_from_string(colorStr);
			
			/* skip argument and param(s) */
			i += 4;
		}
		else if (ARGMATCH("o", "only"))
		{
			only = PARAM(0);
			
			if (!only)
				die("arg %s not enough arguments", this);
		}
		else if (ARGMATCH("s", "simple"))
		{
			const char *precisionStr = PARAM(0);
			const char *colorStr = PARAM(1);
			int precision;
			unsigned int color;
			int k;
			
			if (!precisionStr || !colorStr)
				die("arg %s not enough arguments", this);
			
			precision = get_precision_from_string(precisionStr);
			color = get_color_from_string(colorStr);
			
			for (k = 0; k < DPIMODE_COUNT; ++k)
			{
				struct dpimode *mode;
				
				mode = &dpimode[k];
				mode->precision = precision;
				mode->color = color;
			}
			
			/* skip argument and param(s) */
			i += 3;
		}
		else
			die("unknown argument '%s'", this);
#undef ARGMATCH
#undef PARAM
	}
	
	hp = harpoon_new();
	
	if ((errstr = harpoon_connect(hp)))
		die("%s", errstr);
	
	/* change the mouse's polling rate */
	if (polling)
		harpoon_send(hp, harpoonPacket_pollrate(1000 / polling));
	
	/* apply any DPI settings the user has requested */
	for (i = 0; i < DPIMODE_COUNT; ++i)
	{
		struct dpimode *mode = &dpimode[i];
		int precision;
		unsigned int color;
		int index = i;
		
		precision = mode->precision;
		color = mode->color;
		
		if (!precision)
			continue;
		
		/* tell the mouse all about it */
		harpoon_send(hp, harpoonPacket_dpiconfig(
			index
			, precision /* x, y */
			, precision
			, color >> 16 /* r, g, b */
			, color >> 8
			, color
		));
		harpoon_send(hp, harpoonPacket_dpimode(index)); /* use new mode */
	}
	
	/* user wishes to disable any modes not listed in 'only' */
	if (only)
	{
		const char *s;
		char enabled[DPIMODE_COUNT] = {0};
		
		for (s = only; *s; ++s)
		{
			char c;
			
			if (sscanf(s, "%c", &c) != 1
				|| c < '0'
				|| c >= '0' + DPIMODE_COUNT
			)
				die(
					"'%s' invalid mode list, expecting only decimal values 0 - %d"
					, only
					, DPIMODE_COUNT - 1
				);
			
			enabled[c - '0'] = 1;
		}
		
		harpoon_send(hp, harpoonPacket_dpisetenabled(
			enabled[0]
			, enabled[1]
			, enabled[2]
			, enabled[3]
			, enabled[4]
			, enabled[5]
		));
	}
	
	harpoon_delete(hp);
	
	return 0;
}

