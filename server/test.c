#include <stdio.h>

int main(void) {
    char phrase[] = "play elie 3";
    char mot1[20], mot2[20], mot3[20];

    sscanf(phrase, "%s %s %s", mot1, mot2, mot3);

    printf("mot1 = %s\n", mot1);
    printf("mot2 = %s\n", mot2);
    printf("mot3 = %s\n", mot3);

    return 0;
}
