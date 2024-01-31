/**
 * Copyright (c) 2018 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>

#include "bsp.h"
#include "nordic_common.h"
#include "nrf_error.h"


#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#include "app_timer.h"

#include "app_error.h"
#include "app_util.h"

#include "nrf_cli.h"
#include "nrf_cli_rtt.h"
#include "nrf_cli_types.h"

#include "boards.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_cli_libuarte.h"

#include "nrf_drv_clock.h"



// state machine
enum modez_t {START, MUTE_HAND_DOWN, MUTE_HAND_UP, TALK_HAND_DOWN, TALK_HAND_UP, MUTE_LAID_FLAT, TALK_LAID_FLAT, REST};
typedef enum modez_t  modez_t;
enum event_t {NONE, EVT_CLICK_DOWN, EVT_CLICK_UP, EVT_LAY_FLAT, EVT_RAISE, EVT_LOWER};
typedef enum event_t event_t;
enum muteState_t {MUTE, TALK};
typedef enum muteState_t muteState_t;
enum handPosition_t {HAND_DOWN, HAND_UP, LAID_FLAT};
typedef enum handPosition_t handPosition_t;

modez_t mode = START;
event_t event = NONE;
muteState_t muteState = MUTE;
handPosition_t handPosition = HAND_DOWN;

// event = NONE;
// muteState = TALK;
// handPosition = HAND_UP;









/**@brief Function for handling bsp events.
 */
void bsp_evt_handler(bsp_event_t evt)
{
    switch (evt)
    {
        case BSP_EVENT_KEY_0:
            event = EVT_CLICK_DOWN;
            break;

        case BSP_EVENT_KEY_1:
            event = EVT_CLICK_UP;
            break;

        default:
            return; // no implementation needed
    }
    // err_code = bsp_indication_set(actual_state);
    // NRF_LOG_INFO("%s", (uint32_t)indications_list[actual_state]);
    // APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing low frequency clock.
 */
void clock_initialization()
{
        NRF_CLOCK->LFCLKSRC            = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART    = 1;

    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        // Do nothing.
    }
}


/**@brief Function for initializing bsp module.
 */
void bsp_configuration()
{
    uint32_t err_code;

    err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_evt_handler);
    APP_ERROR_CHECK(err_code);
}












/* Counter timer. */
APP_TIMER_DEF(m_timer_0);

static uint32_t m_counter;
static bool m_counter_active = false;

/** @brief Command line interface instance. */
#define CLI_EXAMPLE_LOG_QUEUE_SIZE  (4)

NRF_CLI_LIBUARTE_DEF(m_cli_libuarte_transport, 256, 256);
NRF_CLI_DEF(m_cli_libuarte,
            "clicky_pen_cli:~$ ",
            &m_cli_libuarte_transport.transport,
            '\r',
            CLI_EXAMPLE_LOG_QUEUE_SIZE);

NRF_CLI_RTT_DEF(m_cli_rtt_transport);
NRF_CLI_DEF(m_cli_rtt,
            "clicky_pen_cli:~$ ",
            &m_cli_rtt_transport.transport,
            '\n',
            CLI_EXAMPLE_LOG_QUEUE_SIZE);

static void timer_handle(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    if (m_counter_active)
    {
        m_counter++;
        NRF_LOG_RAW_INFO("counter = %d\n", m_counter);
    }
}

static void cli_start(void)
{
    ret_code_t ret;

    ret = nrf_cli_start(&m_cli_libuarte);
    APP_ERROR_CHECK(ret);

    ret = nrf_cli_start(&m_cli_rtt);
    APP_ERROR_CHECK(ret);
}

static void cli_init(void)
{
    ret_code_t ret;

    cli_libuarte_config_t libuarte_config;
    libuarte_config.tx_pin   = TX_PIN_NUMBER;
    libuarte_config.rx_pin   = RX_PIN_NUMBER;
    libuarte_config.baudrate = NRF_UARTE_BAUDRATE_115200;
    libuarte_config.parity   = NRF_UARTE_PARITY_EXCLUDED;
    libuarte_config.hwfc     = NRF_UARTE_HWFC_DISABLED;
    ret = nrf_cli_init(&m_cli_libuarte, &libuarte_config, true, true, NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);

    ret = nrf_cli_init(&m_cli_rtt, NULL, true, true, NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);
}

static void cli_process(void)
{
    nrf_cli_process(&m_cli_libuarte);
    nrf_cli_process(&m_cli_rtt);
}


int main(void)
{
    ret_code_t ret;





    clock_initialization();

    uint32_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
    NRF_LOG_INFO("BSP example started.");
    bsp_configuration();



    /* Configure board. */
    
        bsp_board_init(BSP_INIT_LEDS);

    APP_ERROR_CHECK(NRF_LOG_INIT(app_timer_cnt_get));

    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);
    nrf_drv_clock_lfclk_request(NULL);

    // ret = app_timer_init();
    // APP_ERROR_CHECK(ret);

    ret = app_timer_create(&m_timer_0, APP_TIMER_MODE_REPEATED, timer_handle);
    APP_ERROR_CHECK(ret);

    ret = app_timer_start(m_timer_0, APP_TIMER_TICKS(1000), NULL);
    APP_ERROR_CHECK(ret);

    cli_init();

    cli_start();

    NRF_LOG_RAW_INFO("Command Line Interface example started.\n");
    NRF_LOG_RAW_INFO("Please press the Tab key to see all available commands.\n");

    while (true)
    {
        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
        cli_process();
    }
}

static void led_toggle(int led_num)
{
    /* Toggle LED. */        
    bsp_board_led_invert(led_num);
    // nrf_delay_ms(500);
}

void talk(void) {
    // turn on audio
    bsp_board_led_on(3);
    bsp_board_led_off(0);
    // led_toggle(3);
    // led_toggle(0); 
}

void mute(void) {
    // turn off audio
    bsp_board_led_off(3);
    bsp_board_led_on(0);

    // led_toggle(3);
    // led_toggle(0);
}

void raiseHand(void) {
    // raise hand
    led_toggle(1);
}

void lowerHand(void) {
    // lower hand
    led_toggle(1);
}

void layFlat(void) {
    // lay hand flat
    led_toggle(2);
}



void stateMachine(void) {
    // if (event && muteState && handPosition) {
    //     // statements
    //     printf("what is up, love the statemachine\n");
    //     // talk();
    // }
    // start state machine
    while(1) {
        // look for event

        // switch between machine states
        switch (mode)
        {
            case START:
            // determine app state and switch to initial state
                mode = MUTE_HAND_DOWN;

            break;

            case MUTE_HAND_DOWN:
            // statements
                if (event == NONE)
                {   // chill
                    break;
                }
                if (event == EVT_CLICK_DOWN)
                {   // switch to talk state
                    talk();
                    mode = TALK_HAND_DOWN;
                    event = NONE;
                    break;
                }
                if (event == EVT_CLICK_UP)
                {   // mistake / misalignment of state with reality
                    event = NONE;
                    break;
                }
                if (event == EVT_LAY_FLAT)
                {   // switch to mute laid flat state
                    layFlat();
                    mode = MUTE_LAID_FLAT;
                    event = NONE;
                    break;
                }
                if (event == EVT_RAISE)
                {   // switch to mute hand up state
                    raiseHand();
                    mode = MUTE_HAND_UP;
                    event = NONE;
                    break;
            
                }

            break;

            case MUTE_HAND_UP:
            // statements
                if (event == EVT_CLICK_DOWN)
                {   // time to talk
                    talk();
                    mode = TALK_HAND_UP;
                    event = NONE;
                    break;
                }
                if (event == EVT_CLICK_UP)
                {   // mistake / misalignment of state w/ reality
                    event = NONE;
                    break;
                }
                if (event == EVT_LAY_FLAT)
                {   // switch to mute laid flat state
                    layFlat();
                    mode = MUTE_LAID_FLAT;
                    event = NONE;
                    break;
                }
                if (event == EVT_RAISE)
                {   // switch to mute hand up state
                    raiseHand();
                    mode = MUTE_HAND_UP;
                    event = NONE;
                    break;
            
                }
            break;

            case TALK_HAND_DOWN:
            // statements
                if (event == EVT_CLICK_DOWN)
                {   // mistake / misalignment of state w/ reality
                    event = NONE;
                    break;
                }
                if (event == EVT_CLICK_UP)
                {   // time to mute
                    mute();
                    mode = MUTE_HAND_DOWN;
                    event = NONE;
                    break;
                }
                if (event == EVT_LAY_FLAT)
                {   // switch to mute laid flat state
                    layFlat();
                    mode = MUTE_LAID_FLAT;
                    event = NONE;
                    break;
                }
                if (event == EVT_RAISE)
                {   // switch to mute hand up state
                    raiseHand();
                    mode = TALK_HAND_UP;
                    event = NONE;
                    break;
            
                }
            break;

            case TALK_HAND_UP:
            // statements

            break;

            case MUTE_LAID_FLAT:
            // statements

            break;

            case TALK_LAID_FLAT:
            // statements

            break;

            case REST:
            // statements

            break;

            // default:
            // default statements

        }
    }

    // return 0;
}





static void cmd_hand(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    if (nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    led_toggle(3);

    // nrf_cli_fprintf(p_cli, NRF_CLI_OPTION,
    //                 "--raise\n"
    //                 "--lower\n");

    nrf_cli_fprintf(p_cli,NRF_CLI_NORMAL,
                    "Hand is...\n\n");
};

static void cmd_audio(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    if (nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    led_toggle(0);

    // nrf_cli_fprintf(p_cli, NRF_CLI_OPTION,
    //                 "--raise\n"
    //                 "--lower\n");

    nrf_cli_fprintf(p_cli,NRF_CLI_NORMAL,
                    "Audio is...\n\n");
};


static void cmd_statemachine(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    if (nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    stateMachine();

    // nrf_cli_fprintf(p_cli, NRF_CLI_OPTION,
    //                 "--raise\n"
    //                 "--lower\n");

    nrf_cli_fprintf(p_cli,NRF_CLI_NORMAL,
                    "Starting state machine...\n\n");
};

// static void cmd_wtf(nrf_cli_t const * p_cli, size_t argc, char **argv)
// {   
//     UNUSED_PARAMETER(argc);
//     UNUSED_PARAMETER(argv);

//     if (nrf_cli_help_requested(p_cli))
//     {
//         nrf_cli_help_print(p_cli, NULL, 0);
//         return;
//     }

//     nrf_cli_fprintf(p_cli, NRF_CLI_OPTION,
//                     "\n"
//                     "WTF\n"
//                     "\n");

//     nrf_cli_fprintf(p_cli,NRF_CLI_NORMAL,
//                     "                Dustin              \n\n");
    


// }





// static void led_toggle_one()
// {
//     led_toggle(0);
// }
// static void led_toggle_two()
// {
//     led_toggle(1);
// }
// static void led_toggle_three()
// {
//     led_toggle(2);
// }
// static void led_toggle_four()
// {
//     led_toggle(3);
// }



/** @brief Command set array. */
// NRF_CLI_CMD_REGISTER(nordic, NULL, "Print Nordic Semiconductor logo.", cmd_nordic);

// NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_wtf)
// {
//     NRF_CLI_CMD(general,   NULL, "Print all entered parameters.", cmd_wtf),
//     NRF_CLI_CMD(one, NULL, "Print each parameter in new line.", led_toggle_one),
//     NRF_CLI_CMD(two, NULL, "Print each parameter in new line.", led_toggle_two),
//     NRF_CLI_CMD(three, NULL, "Print each parameter in new line.", led_toggle_three),
//     NRF_CLI_CMD(four, NULL, "Print each parameter in new line.", led_toggle_four),

//     NRF_CLI_SUBCMD_SET_END
// };
// NRF_CLI_CMD_REGISTER(wtf, &m_sub_wtf, "Print WTF.", cmd_wtf);

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_hand)
{
    NRF_CLI_CMD(raise,   NULL, "Print all entered parameters.", cmd_hand),
    NRF_CLI_CMD(lower, NULL, "Print each parameter in new line.", cmd_hand),

    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(hand, &m_sub_hand, "Raise or lower hand.", cmd_hand);


NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_audio)
{
    NRF_CLI_CMD(raise,   NULL, "Print all entered parameters.", cmd_hand),
    NRF_CLI_CMD(lower, NULL, "Print each parameter in new line.", cmd_hand),

    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(mute, &m_sub_audio, "Raise or lower hand.", cmd_audio);

NRF_CLI_CMD_REGISTER(gostatemachinego, NULL, "Start state machine", cmd_statemachine);


