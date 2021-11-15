mkdir -p bin/linux

gcc -o bin/linux/harpoon -Wall -Wextra -DHARPOON_NO_MAIN_LOOP src/harpoon.c src/cli.c -lusb-1.0

gcc -o bin/linux/harpoon-monitor -Wall -Wextra src/harpoon.c src/monitor.c -lusb-1.0

