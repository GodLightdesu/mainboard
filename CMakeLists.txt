# ============================================================================
# STM32H750 DSP 项目 CMake 配置文件
# 目标: STM32H750 + CMSIS-DSP + CMake + VSCode
# ============================================================================

cmake_minimum_required(VERSION 3.22)

# ============================================================================
# 基础配置
# ============================================================================

# C语言标准设置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# 构建类型设置 (Debug/Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# 项目名称
set(CMAKE_PROJECT_NAME mainboard)

# 生成编译命令数据库，便于clangd等工具进行代码跳转
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# 项目初始化
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# 启用 C 和 ASM 语言支持
enable_language(C ASM)

# 创建可执行文件目标
add_executable(${CMAKE_PROJECT_NAME})

# ============================================================================
# STM32CubeMX 自动生成的代码
# 注意: 此子目录包含HAL库、启动文件、链接脚本等基础配置
# ============================================================================
add_subdirectory(cmake/stm32cubemx)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE

    # Add user defined library search paths
)

# ============================================================================
# CMSIS-DSP 配置
# ============================================================================

# 仅包含需要的快速数学函数源文件，以减小代码体积
set(CMSIS_DSP_SOURCES
    Libraries/Dsp/Source/FastMathFunctions/arm_sin_f32.c # 正弦函数
    Libraries/Dsp/Source/FastMathFunctions/arm_cos_f32.c # 余弦函数
    Libraries/Dsp/Source/FastMathFunctions/arm_atan2_f32.c # 反正切函数
    Libraries/Dsp/Source/CommonTables/arm_common_tables.c # 公共查找表
    Libraries/Dsp/Source/CommonTables/arm_const_structs.c # 常量结构体
)

# ============================================================================
# 源文件配置
# ============================================================================
target_sources(${CMAKE_PROJECT_NAME} PRIVATE

    # 用户应用源文件
    Core/Src/main.c # 主程序入口
    Libraries/I2C/i2c_common.c # I2C通信模块
    Libraries/Uart/dataPrint.c # UART数据收发
    Libraries/Button/button.c # 按键处理模块
    Libraries/LED/led.c # LED控制模块

    # Libraries/Module/MPU6050/MPU6050.c # MPU6050传感器驱动
    # Libraries/Module/MPU6050/MPU6050_DMP.c # MPU6050 DMP功能
    Libraries/Module/MPU6050/MPU6050DMP.c # MPU6050传感器驱动
    Libraries/Module/MPU6050/MotionDriver/inv_mpu_dmp_motion_driver.c # DMP驱动
    Libraries/Module/MPU6050/MotionDriver/inv_mpu.c # MPU核心驱动

    Libraries/Module/Cam/cam.c # 摄像头模块
    Libraries/Module/Xsound/xsound.c # 声音模块
    Libraries/Module/Encoder/encoder.c # 编码器模块
    Libraries/Module/Motors/motors.c # 电机控制
    Libraries/Module/Grayscale/grayscale.c # 灰度传感器
    Libraries/Module/IR/ir.c # 红外传感器
    Libraries/PID/PID.c # PID控制器
    Libraries/soccer/soccer.c # 主控制逻辑
    Libraries/Soccer/attack/attack.c # 进攻模式实现
    Libraries/Soccer/defense/defense.c # 防守模式实现

    # CMSIS-DSP 源文件
    ${CMSIS_DSP_SOURCES}
)

# ============================================================================
# 头文件包含路径
# ============================================================================
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE

    # 用户自定义头文件路径
    Core/Inc # 主程序头文件
    Libraries/Soccer # 足球机器人状态机头文件
    Libraries/Soccer/attack # 进攻模式头文件
    Libraries/Soccer/defense # 防守模式头文件
    Libraries/Button # 按键模块头文件
    Libraries/PID # PID控制器头文件
    Libraries/Module/IR # 红外传感器头文件
    Libraries/Module/MPU6050 # MPU6050传感器头文件
    Libraries/Module/MPU6050/MotionDriver # DMP驱动头文件
    Libraries/Module/Cam # 摄像头头文件
    Libraries/Module/Xsound # 声音模块头文件
    Libraries/Module/Motors # 电机控制头文件
    Libraries/Module/Encoder # 编码器头文件
    Libraries/Module/Grayscale # 灰度传感器头文件
    Libraries/Uart # UART相关头文件
    Libraries/I2C # I2C相关头文件
    Libraries/LED # LED控制头文件
    Libraries/ # 库根目录

    # STM32 HAL 驱动头文件
    Drivers/STM32H7xx_HAL_Driver/Inc

    # CMSIS 核心头文件
    Drivers/CMSIS/Device/ST/STM32H7xx/Include # 设备特定定义
    Drivers/CMSIS/Include # ARM CMSIS核心

    # CMSIS-DSP 头文件
    Libraries/Dsp/Include
    Libraries/Dsp/PrivateInclude
)

# ============================================================================
# 预处理器宏定义
# ============================================================================
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE

    # ==================== 用户调试宏 ====================
    # DEBUG_GENERAL # 启用通用调试信息输出
    DEBUG_BUTTON # 启用按键调试信息输出

    # DEBUG_SOCCER # 启用足球机器人状态机调试信息输出
    # DEBUG_ATTACK # 启用进攻模式调试信息输出
    # DEBUG_DEFENSE # 启用防守模式调试信息输出

    # DEBUG_CAM # 启用摄像头调试信息

    # DEBUG_IR # 启用红外传感器调试信息
    # DEBUG_XS # 启用Xsound传感器调试信息
    # DEBUG_GRAYSCALE # 启用灰度传感器调试信息

    # DEBUG_MPU6050        # MPU6050传感器数据和初始化调试
    # DEBUG_MPU6050_DMP # MPU6050姿态（滚转/俯仰/偏航）调试
    # DEBUG_I2C # I2C通信错误和状态调试

    # DEBUG_MOTORS         # 电机测试PWM值调试

    # ==================== STM32H750 必需宏 ====================
    USE_HAL_DRIVER # 启用HAL库
    STM32H750xx # 目标芯片型号

    # ==================== CMSIS-DSP 必需宏 ====================
    ARM_MATH_CM7 # 启用Cortex-M7 DSP优化

    # 注意: 不要定义 __FPU_PRESENT 和 __FPU_USED
    # 这些宏已经在 stm32h750xx.h 和 core_cm7.h 中定义
)

# ============================================================================
# 编译器选项
# ============================================================================
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE

    # CPU架构设置
    -mcpu=cortex-m7 # Cortex-M7内核
    -mthumb # Thumb指令集

    # 浮点单元设置 (STM32H750 单精度FPU)
    -mfpu=fpv5-d16 # FPv5-D16单精度浮点单元
    -mfloat-abi=hard # 硬件浮点ABI（必须为hard才能使用FPU）

    # 优化级别
    -O2 # 优化级别2（性能与代码大小的平衡）

    # 可选: -Ofast         # 最高性能优化（可能影响浮点精度）

    # 代码大小优化
    -ffunction-sections # 每个函数单独成段
    -fdata-sections # 每个数据单独成段

    # 警告选项
    -Wall # 启用所有警告
    -Wextra # 启用额外警告
)

# ============================================================================
# 链接器选项
# ============================================================================
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE

    # CPU架构设置（必须与编译器选项一致）
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard

    # 标准库设置
    # 注意: 不指定 -specs=nano.specs 避免命名冲突
    # 使用默认的标准库配置

    # 特殊功能支持
    -u_printf_float # 启用printf浮点数支持

    # 代码优化
    -Wl,--gc-sections # 移除未使用的代码段（与 -ffunction-sections 配合）
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map # 生成内存映射文件

    # 注意: 链接脚本在 cmake/stm32cubemx 子目录中已定义
    # 不要在此重复指定 -T 链接脚本选项
)

# ============================================================================
# 链接库配置
# ============================================================================

# 移除错误的库依赖（处理CMake的隐式链接库问题）
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob) # 添加项目依赖的库
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx # STM32CubeMX生成的库（包含HAL、启动文件等）
    m # 标准数学库（libm）

    # Add user defined libraries
)

# ============================================================================
# 使用说明
# ============================================================================
# 1. 构建命令:
# mkdir -p build && cd build
# cmake -DCMAKE_BUILD_TYPE=Release ..
# make -j4
#
# 2. 调试DSP函数:
# - 在main.c中添加: #include "arm_math.h"
# - 确保FPU已启用: SCB->CPACR |= (0xF << 20);
# - 使用DSP函数: arm_sin_f32(&angle, &result);
#
# 3. 验证构建:
# arm-none-eabi-size mainboard.elf   # 查看代码大小
# arm-none-eabi-nm mainboard.elf | grep arm_sin # 验证DSP符号
#
# 4. 常见问题:
# - FPU宏重复定义: 删除 __FPU_PRESENT 和 __FPU_USED 定义
# - 链接脚本重复: 确保只在 cmake/stm32cubemx 中定义一次
# - nano.specs冲突: 不要使用 -specs=nano.specs
# ============================================================================