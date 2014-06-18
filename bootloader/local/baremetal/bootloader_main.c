/*
 * This file is part of the KeepKey project.
 *
 * Copyright (C) 2014 Carbon Design Group <tom@carbondesign.com> All rights reserved.
 * Developed for KeepKey by Carbon Design Group.
 */

/**
 * The bootloader implements a robust firmware update scheme over the
 * KeepKey USB interface.  The intent is to support both in-field upgrades
 * by users, as well as factory load.
 *
 * Features
 *   * Validate firmware image signature against the KeepKey private signature.
 *   * Supports a single firmware image, with unlimited update retries.
 *
 * Notes
 *   * No support is provided for updating the bootloader remotely.  The idea is
 *     to keep the bootloader as simple as possible in order to allow us to
 *     load the firmware at the manufacturer, and not touch it again.
 *   
 *   * There is only a single firmware load, so folks who fail a firmware load
 *     due to whatever reason will need to reset the device and retry.
 *
 */

//================================ INCLUDES =================================== 
//
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/f2/rng.h>

#include <keepkey_board.h>
#include <keepkey_display.h>
#include <keepkey_leds.h>
#include <keepkey_button.h>
#include <timer.h>
#include <layout.h>

#include <confirm_sm.h>
#include <keepkey_led.h>
#include <usb_driver.h>
#include <usb_flash.h>


//====================== CONSTANTS, TYPES, AND MACROS =========================

typedef void (*app_entry_t)(void);

//=============================== VARIABLES ===================================

uint32_t * const  SCB_VTOR = (uint32_t*)0xe000ed08;

//====================== PRIVATE FUNCTION DECLARATIONS ========================


//=============================== FUNCTIONS ===================================

static bool validate_firmware()
{
    return false;
}

/**
 * Lightweight routine to reset the vector table to point to the application's vector table.
 *
 * @param offset This must be a multiple of 0x200.  This is added to to the base address of flash
 *               in order to compute the correct base address.
 * 
 */
static void set_vector_table_offset(uint32_t offset)
{ 
    static const uint32_t NVIC_OFFSET_FLASH = ((uint32_t)0x08000000);

    *SCB_VTOR = NVIC_OFFSET_FLASH | (offset & (uint32_t)0x1FFFFF80);
}

static void boot_jump(uint32_t addr)
{
    /*
     * Jump to one after the base app address to get past the stack pointer.  The +1 
     * is to maintain a valid thumb instruction.
     */
    uint32_t entry_addr = addr+4;
    uint32_t app_entry_addr = (uint32_t)(*(uint32_t*)(entry_addr));
    app_entry_t app_entry = (app_entry_t)app_entry_addr;
    app_entry();
}

void blink(void* context)
{
    toggle_red();    
}



static void configure_hw()
{
#define Y
#ifdef X
   clock_scale_t clock = hse_8mhz_3v3[ CLOCK_3V3_120MHZ ];
    rcc_clock_setup_hse_3v3( &clock );

    // Enable GPIOA/B/C clock.
    rcc_periph_clock_enable( RCC_GPIOA );
    rcc_periph_clock_enable( RCC_GPIOB );
    rcc_periph_clock_enable( RCC_GPIOC );

    // Enable the peripheral clock for the system configuration 
    rcc_peripheral_enable_clock( &RCC_APB2ENR, RCC_APB2ENR_SYSCFGEN );

    // Enable the periph clock for timer 4
    rcc_peripheral_enable_clock( &RCC_APB1ENR, RCC_APB1ENR_TIM4EN );

    timer_init();

    keepkey_leds_init();

    keepkey_button_init();

    post_periodic(&blink, NULL, 1000, 1000);
    while(1) {}


    return;

#endif
#ifdef Y


    clock_scale_t clock = hse_8mhz_3v3[CLOCK_3V3_120MHZ];
    rcc_clock_setup_hse_3v3(&clock);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_OTGFS);
    rcc_periph_clock_enable(RCC_SYSCFG);
    rcc_periph_clock_enable(RCC_TIM4);

    timer_init();

    keepkey_leds_init();

    led_init();

    keepkey_button_init();

    display_init();

    layout_init( display_canvas() );
#endif
}

bool check_firmware_sig(void)
{
    return true;
}

const char* firmware_sig_as_string(void)
{
    return "KL123123KJH3452";
}

void board_reset(void)
{
    while(1) {}
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    configure_hw();

    bool update_mode = keepkey_button_down();

    led_green(true);
    led_red(true);



    while(1)
    {
        if(validate_firmware() && !update_mode)
        {
            if(check_firmware_sig())
            {
                layout_standard_notification("Loading KeepKey firmware", firmware_sig_as_string());
            } else {
                layout_standard_notification("UNSIGNED FIRMWARE", firmware_sig_as_string());
            }

            delay(5000);

            led_red(false);
            set_vector_table_offset(0x10000);
            boot_jump(0x08010000);
        } else {
            led_green(false);

            if(update_mode)
            {
                if(confirm("Hold button to confirm flash update."))
                {
                    usb_flash_firmware();
                    board_reset();
                } 
            } else {
                layout_standard_notification("Invalid firmware image detected.", "Reset and perform a firmware update.");
                display_refresh();
                break;
            }
        }
    }

    while(1) {}

    return 0;
}