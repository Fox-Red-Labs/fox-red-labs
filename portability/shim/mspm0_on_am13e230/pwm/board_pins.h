/* What SysConfig would generate for the timer and its pins: TIMG4 on PA3 (CCP0) and PA4 (CCP1). */
#pragma once
#define PWM_0_INST TIMG4
#define GPIO_PWM_0_C0_IOMUX      (IOMUX_PINCM_PA3)
#define GPIO_PWM_0_C0_IOMUX_FUNC IOMUX_PA3_TIMG4_0_CCP0
#define GPIO_PWM_0_C0_PORT       GPIO0
#define GPIO_PWM_0_C0_PIN        DL_GPIO_PIN(3)
#define GPIO_PWM_0_C1_IOMUX      (IOMUX_PINCM_PA4)
#define GPIO_PWM_0_C1_IOMUX_FUNC IOMUX_PA4_TIMG4_0_CCP1
#define GPIO_PWM_0_C1_PORT       GPIO0
#define GPIO_PWM_0_C1_PIN        DL_GPIO_PIN(4)
