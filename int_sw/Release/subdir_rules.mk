################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
Adafruit_GFX.obj: ../Adafruit_GFX.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me -Ooff --include_path="C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/include" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/driverlib" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/inc" --define=ccs --define=NON_NETWORK --define=cc3200 -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="Adafruit_GFX.d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

Adafruit_OLED.obj: ../Adafruit_OLED.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me -Ooff --include_path="C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/include" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/driverlib" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/inc" --define=ccs --define=NON_NETWORK --define=cc3200 -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="Adafruit_OLED.d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

main.obj: ../main.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me -Ooff --include_path="C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/include" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/driverlib" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/inc" --define=ccs --define=NON_NETWORK --define=cc3200 -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="main.d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

pin_mux_config.obj: ../pin_mux_config.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me -Ooff --include_path="C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/include" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/driverlib" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/inc" --define=ccs --define=NON_NETWORK --define=cc3200 -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="pin_mux_config.d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

startup_ccs.obj: C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common/startup_ccs.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me -Ooff --include_path="C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/include" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/driverlib" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/inc" --define=ccs --define=NON_NETWORK --define=cc3200 -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="startup_ccs.d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

timer_if.obj: C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common/timer_if.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me -Ooff --include_path="C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/include" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/driverlib" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/inc" --define=ccs --define=NON_NETWORK --define=cc3200 -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="timer_if.d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

uart_if.obj: ../uart_if.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me -Ooff --include_path="C:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.1.LTS/include" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/example/common" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/driverlib" --include_path="C:/ti/CC3200SDK_1.3.0/cc3200-sdk/inc" --define=ccs --define=NON_NETWORK --define=cc3200 -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="uart_if.d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


