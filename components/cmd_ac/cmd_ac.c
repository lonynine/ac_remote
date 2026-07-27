/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmd_ac.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "ac_state.h"
#include "control_task.h"
#include "ir_remote.h"

static const char *mode_to_str(ac_mode_t mode)
{
    switch (mode) {
        case AC_MODE_COOL: return "制冷 (COOL)";
        case AC_MODE_HEAT: return "制热 (HEAT)";
        case AC_MODE_AUTO: return "自动 (AUTO)";
        case AC_MODE_DRY:  return "抽湿 (DRY)";
        case AC_MODE_FAN:  return "送风 (FAN)";
        default: return "未知";
    }
}

static const char *fan_to_str(ac_fan_t fan)
{
    switch (fan) {
        case AC_FAN_AUTO: return "自动风 (AUTO)";
        case AC_FAN_LOW:  return "低风 (LOW)";
        case AC_FAN_MED:  return "中风 (MED)";
        case AC_FAN_HIGH: return "高风 (HIGH)";
        default: return "未知";
    }
}

static void print_ac_status(void)
{
    ac_state_t state;
    ac_state_get(&state);

    printf("\n================ [ 当前空调状态 ] ================\n");
    printf("  电源状态 : %s\n", state.power ? "开 (ON)" : "关 (OFF)");
    printf("  运行模式 : %s\n", mode_to_str(state.mode));
    printf("  设定温度 : %d ℃\n", state.temp);
    printf("  风速等级 : %s\n", fan_to_str(state.fan));
    printf("==================================================\n\n");
}

static int do_ac_cmd(int argc, char **argv)
{
    if (argc < 2) {
        print_ac_status();
        printf("用法提示:\n");
        printf("  ac set [power] [mode] [temp] [fan]   : 设置空调状态 (例如: ac set on cool 25 low)\n");
        printf("  ac send <brand> <power> <mode> <temp> <fan> : 动态品牌发波 (例如: ac send haier/gree/midea/aux on cool 25 low)\n");
        printf("  ac on                               : 快速开启空调\n");
        printf("  ac off                              : 快速关闭空调\n");
        printf("  ac learn [timeout]                  : 开启红外学码功能 (默认等待 5 秒)\n");
        printf("  ac emit                             : 重发上一次成功学到的红外信号\n\n");
        return 0;
    }

    if (strcmp(argv[1], "on") == 0) {
        ac_state_t state;
        ac_state_get(&state);
        state.power = true;
        ac_state_set(&state);
        control_task_send_ac_cmd(&state);
        printf("已成功开启空调！\n");
        print_ac_status();
        return 0;
    }

    if (strcmp(argv[1], "off") == 0) {
        ac_state_t state;
        ac_state_get(&state);
        state.power = false;
        ac_state_set(&state);
        control_task_send_ac_cmd(&state);
        printf("已成功关闭空调！\n");
        print_ac_status();
        return 0;
    }

    if (strcmp(argv[1], "learn") == 0) {
        uint32_t timeout_sec = 5;
        if (argc >= 3) {
            int t = atoi(argv[2]);
            if (t > 0) timeout_sec = (uint32_t)t;
        }
        ir_remote_learn_start(timeout_sec);
        return 0;
    }

    if (strcmp(argv[1], "emit") == 0) {
        ir_remote_learn_emit();
        return 0;
    }

    // 动态品牌发波: ac send <brand> <power> <mode> <temp> <fan>
    // 示例: ac send gree on cool 26 auto
    if (argc >= 6 && strcmp(argv[1], "send") == 0) {
        ac_remote_cmd_t cmd = {0};
        const char *b = argv[2];
        if (strcmp(b, "haier") == 0) cmd.brand = AC_BRAND_HAIER;
        else if (strcmp(b, "gree") == 0)  cmd.brand = AC_BRAND_GREE;
        else if (strcmp(b, "midea") == 0) cmd.brand = AC_BRAND_MIDEA;
        else if (strcmp(b, "aux") == 0)   cmd.brand = AC_BRAND_AUX;
        else cmd.brand = AC_BRAND_HAIER;

        cmd.power = (strcmp(argv[3], "on") == 0 || strcmp(argv[3], "1") == 0);
        
        const char *m = argv[4];
        if (strcmp(m, "cool") == 0)      cmd.mode = AC_MODE_COOL;
        else if (strcmp(m, "heat") == 0) cmd.mode = AC_MODE_HEAT;
        else if (strcmp(m, "auto") == 0) cmd.mode = AC_MODE_AUTO;
        else if (strcmp(m, "dry") == 0)  cmd.mode = AC_MODE_DRY;
        else if (strcmp(m, "fan") == 0)  cmd.mode = AC_MODE_FAN;

        int t = atoi(argv[5]);
        cmd.temp = (t >= 16 && t <= 30) ? (uint8_t)t : 25;

        if (argc >= 7) {
            const char *f = argv[6];
            if (strcmp(f, "low") == 0)       cmd.fan = AC_FAN_LOW;
            else if (strcmp(f, "med") == 0)  cmd.fan = AC_FAN_MED;
            else if (strcmp(f, "high") == 0) cmd.fan = AC_FAN_HIGH;
            else if (strcmp(f, "auto") == 0) cmd.fan = AC_FAN_AUTO;
        }

        ir_remote_send_cmd(&cmd);
        printf("已通过【%s】品牌红外协议发送控制指令！\n", b);
        return 0;
    }

    // 默认位置参数控制: ac set <power> <mode> <temp> <fan>
    if (argc >= 5 && strcmp(argv[1], "set") == 0) {
        ac_state_t state;
        ac_state_get(&state);

        const char *p = argv[2];
        if (strcmp(p, "on") == 0 || strcmp(p, "1") == 0) state.power = true;
        else if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) state.power = false;

        const char *m = argv[3];
        if (strcmp(m, "cool") == 0)      state.mode = AC_MODE_COOL;
        else if (strcmp(m, "heat") == 0) state.mode = AC_MODE_HEAT;
        else if (strcmp(m, "auto") == 0) state.mode = AC_MODE_AUTO;
        else if (strcmp(m, "dry") == 0)  state.mode = AC_MODE_DRY;
        else if (strcmp(m, "fan") == 0)  state.mode = AC_MODE_FAN;

        int t = atoi(argv[4]);
        if (t >= 16 && t <= 30) state.temp = (uint8_t)t;

        if (argc >= 6) {
            const char *f = argv[5];
            if (strcmp(f, "low") == 0)       state.fan = AC_FAN_LOW;
            else if (strcmp(f, "med") == 0)  state.fan = AC_FAN_MED;
            else if (strcmp(f, "high") == 0) state.fan = AC_FAN_HIGH;
            else if (strcmp(f, "auto") == 0) state.fan = AC_FAN_AUTO;
        }

        ac_state_set(&state);
        control_task_send_ac_cmd(&state);

        printf("已成功设置并发送空调状态！\n");
        print_ac_status();
        return 0;
    }

    print_ac_status();
    return 0;
}

esp_err_t cmd_ac_register(void)
{
    const esp_console_cmd_t cmd = {
        .command = "ac",
        .help = "空调状态控制与红外发波/学码命令",
        .hint = NULL,
        .func = &do_ac_cmd,
        .argtable = NULL
    };
    return esp_console_cmd_register(&cmd);
}
