#include <stdio.h>
int main(){
    const char *a[3];
    a[0] = "192.168.0.1";
    a[1] = "10.2.3.1";
    a[2] = "10.2.3.1";

    for(int i=0;i<3;i++){
        printf("Element %d: %s\n", i, a[i]);
    }
    
    a[0] = "1.2.3.4";
    a[2] = "41.32.25.194";
    for(int i=0;i<3;i++){
        printf("Element %d: %s\n", i, a[i]);
    }
}
