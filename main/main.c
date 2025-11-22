#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

void app_main(void)
{
    while (true) {
        printf("\033[36mIhor - GAY!\033[31m\nWadzik - GAYDESTROYER!\033[0m\n");
        sleep(1);
    }
}
