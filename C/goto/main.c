#include <stdio.h>

int main(void)
{
    int n;

    printf("Input a number: ");
    scanf("%d", &n);

    if (n < 0)
        goto invalid;

    printf("%d is valid.\n", n);
    goto done;

invalid:
    printf("%d is invalid.\n", n);

done:
    printf("Bye.\n");
    return 0;
}
