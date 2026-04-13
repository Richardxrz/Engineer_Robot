#include "custom_controller.h"
#include <stdlib.h>

CustomControllerApi_t custom_controller = {
    .Blank = NULL,
};

CCControlData_t cc_control_data = {
    .pos = {0},
};

/**
 * @brief  自定义控制器数据包解析
 * @note   
 * @retval None
 */
void CustomControllerObserver(void)
{
    // TODO: 从裁判系统数据包中提取电位器数据
    // 1. 获取自定义控制器数据包
    // 2. 解析6个关节的电位器ADC值（0-4095）
    // 3. 转换为角度值（映射到关节限位范围）
}
/*------------------------------ End of File ------------------------------*/
