#include <wup/wup.h>


extern int dhd_module_init();
extern int bcmsdh_probe(struct device *dev);
extern int bcmsdh_remove(struct device *dev);

//struct device* zatt;

int Wifi_Init()
{
    printf("Wifi_Init\n");

    /*int ret2 = dhd_module_init();
    int ret = bcmsdh_probe(NULL);

    printf("ret2=%d\n", ret2);
    printf("ret=%d\n", ret);
    return ret;*/
    return 0;
}
