/*
    This file is part of Sylverant Flashrom Tool
    Copyright (C) 2018 Lawrence Sebald

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <kos/fs.h>
#include <dc/video.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>

#include "fb_console.h"
#include "flashrom.h"

extern void fprint_buf(FILE *fp, const unsigned char *pkt, int len);

static uint32_t wait_for_input(void) {
    maple_device_t *dev;
    cont_state_t *state;

    for(;;) {
        if((dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER)) != NULL) {
            state = (cont_state_t *)maple_dev_status(dev);

            if(state == NULL)
                thd_pass();

            if(state->buttons)
                return state->buttons;
        }

        thd_pass();
    }
}

static void disclaimer(void) {
    int i;
    uint32_t buttons;

    fb_init();

    vid_waitvbl();

    /* Clear the background to an attention grabbing shade of red... */
    for(i = 0; i < 640 * 480; ++i) {
        vram_s[i] = 0x8000;
    }

    fb_write_string("\n\n");
    fb_write_string("Disclaimer:\n"
                    "This program writes to the flashrom of the\n"
                    "console which, like any flashrom, can only\n"
                    "be rewritten a limited number of times.\n"
                    "The author of this program takes no\n"
                    "responsibility if it breaks your console\n"
                    "or otherwise renders it unusable in some way.\n"
                    "If you do not agree, please exit this program\n"
                    "now.\n\n"
                    "This program comes with ABSOLUTELY NO WARRANTY.\n\n"
                    "Press the Y button to continue or B to exit.\n");

    for(;;) {
        buttons = wait_for_input();

        if((buttons & CONT_B))
            arch_exit();
        else if((buttons & CONT_Y))
            break;
    }

    fb_write_string("Starting...\n");
    thd_sleep(1000);
}

static void draw_base_ui(void) {
    int i;

    /* Reset the console... */
    fb_init();

    /* Wait for vblank. */
    vid_waitvbl();

    /* Clear the background to a nice shade of blue... */
    for(i = 0; i < 640 * 480; ++i) {
        vram_s[i] = 0x0010;
    }

    fb_write_string("Dreamcast Flashrom Tool\n\n");
}

static void debug_menu(void) {
    FILE *fp;
    file_t fh;
    uint8_t *part;
    int len;
    uint32_t buttons;

restart_menu:
    draw_base_ui();

    fb_write_string("Debug Menu\n"
                    "These will only work over dcload!\n\n");
    fb_write_string("A: Dump flashrom to /pc/tmp\n"
                    "B: Dump B1\n"
                    "X: Dump Settings\n"
                    "Y: Dump PSO Saves to /pc/tmp\n"
                    "START: Return\n");

    for(;;) {
        buttons = wait_for_input();

        if((buttons & CONT_START)) {
            fb_write_string("Returning to main menu\n");
            thd_sleep(2000);
            return;
        }
        else if((buttons & CONT_A)) {
            fb_write_string("Dumping flashrom to /pc/tmp/dc_flash.bin\n");
            if(!(fp = fopen("/pc/tmp/dc_flash.bin", "wb"))) {
                fb_write_string("Error opening file\n");
                thd_sleep(1000);
                goto restart_menu;
            }

            fwrite((void *)0x200000, 1, 0x20000, fp);
            fclose(fp);
            fb_write_string("Done\n");
            thd_sleep(2000);
            goto restart_menu;
        }
        else if((buttons & CONT_B)) {
            if(read_partition(FLASHROM_PT_BLOCK_1, &part, &len) < 0) {
                fb_write_string("Error reading partition");
                thd_sleep(1000);
                goto restart_menu;
            }

            fb_write_string("Writing to console...\n");
            printf("-----------------------------\n"
                   "Partition: Block 1\n"
                   "Size: %d bytes\n"
                   "-----------------------------\n", len);
            fprint_buf(stdout, part, len);
            free(part);
            fb_write_string("Done\n");
            thd_sleep(2000);
            goto restart_menu;
        }
        else if((buttons & CONT_X)) {
            if(read_partition(FLASHROM_PT_SETTINGS, &part, &len) < 0) {
                fb_write_string("Error reading partition");
                thd_sleep(1000);
                goto restart_menu;
            }

            fb_write_string("Writing to console...\n");
            printf("-----------------------------\n"
                   "Partition: Settings\n"
                   "Size: %d bytes\n"
                   "-----------------------------\n", len);
            fprint_buf(stdout, part, len);
            free(part);
            fb_write_string("Done\n");
            thd_sleep(2000);
            goto restart_menu;
        }
        else if((buttons & CONT_Y)) {
            fb_write_string("Dumping PSO System File\n");
            if((fh = fs_open("/vmu/a1/PSO______SYS", O_RDONLY)) >= 0) {
                len = (int)fs_total(fh);
                part = fs_mmap(fh);

                if(!(fp = fopen("/pc/tmp/PSO______SYS", "wb"))) {
                    fb_write_string("Error opening output file...\n");
                    thd_sleep(1000);
                    fs_close(fh);
                    goto restart_menu;
                }

                fwrite(part, 1, len, fp);
                fclose(fp);
                fs_close(fh);
            }
            else {
                fb_write_string("System file not found!\n");
            }

            fb_write_string("Dumping PSOv1 Guild Card File\n");
            if((fh = fs_open("/vmu/a1/PSO______GCD", O_RDONLY)) >= 0) {
                len = (int)fs_total(fh);
                part = fs_mmap(fh);

                if(!(fp = fopen("/pc/tmp/PSO______GCD", "wb"))) {
                    fb_write_string("Error opening output file...\n");
                    thd_sleep(1000);
                    fs_close(fh);
                    goto restart_menu;
                }

                fwrite(part, 1, len, fp);
                fclose(fp);
                fs_close(fh);
            }
            else {
                fb_write_string("V1 Guild Card file not found!\n");
            }

            fb_write_string("Dumping PSOv2 Guild Card File\n");
            if((fh = fs_open("/vmu/a1/PSO______2GC", O_RDONLY)) >= 0) {
                len = (int)fs_total(fh);
                part = fs_mmap(fh);

                if(!(fp = fopen("/pc/tmp/PSO______2GC", "wb"))) {
                    fb_write_string("Error opening output file...\n");
                    thd_sleep(1000);
                    fs_close(fh);
                    goto restart_menu;
                }

                fwrite(part, 1, len, fp);
                fclose(fp);
                fs_close(fh);
            }
            else {
                fb_write_string("V2 Guild Card file not found!\n");
            }

            fb_write_string("Done\n");
            thd_sleep(2000);
            goto restart_menu;
        }
    }
}

static void main_menu(void) {
    uint32_t buttons = 0, last_buttons = 0;
    uint32_t v1 = 0, v2 = 0;
    int rv;
    char buf[50];

restart_menu:
    draw_base_ui();

    fb_write_string("Press A to display PSO Serial Numbers\n"
                    "Press B to erase PSO Serial Numbers\n"
                    "Press X to erase the entire flashrom\n"
                    "Press START to exit\n");

    for(;;) {
        buttons = wait_for_input();

        if((buttons & CONT_START)) {
            return;
        }
        else if((buttons & (CONT_A | CONT_Y)) == (CONT_A | CONT_Y)) {
            /* SUPER SECRET DEBUG MENU! */
            thd_sleep(2000);
            debug_menu();
            last_buttons = 0;
            goto restart_menu;
        }
        else if((buttons & CONT_A) && !(last_buttons & CONT_A)) {
            rv = find_pso_keys(&v1, &v2);
            fb_write_string("\n\n");

            if(rv < 0) {
                fb_write_string("No PSO Serial Numbers Found!\n");
            }
            else {
                if(v1 && v1 != 0xffffffff) {
                    sprintf(buf, "PSOv1 Serial Number: %" PRIX32 "\n", v1);
                    fb_write_string(buf);
                }
                else {
                    fb_write_string("No PSOv1 Serial Number Found\n");
                }

                if(v2 && v2 != 0xffffffff) {
                    sprintf(buf, "PSOv2 Serial Number: %" PRIX32 "\n", v2);
                    fb_write_string(buf);
                }
                else {
                    fb_write_string("No PSOv2 Serial Number Found\n");
                }
            }

            last_buttons = buttons;
        }
        else if((buttons & CONT_B) && !(last_buttons & CONT_B)) {
            fb_write_string("\n\n");

            fb_write_string("Are you sure you wish to erase your PSO Serial\n"
                            "Numbers?\n"
                            "Press A + B to confirm, START to Cancel.\n"
                            "This cannot be undone!\n");

            for(;;) {
                buttons = wait_for_input();

                if((buttons & CONT_START)) {
                    fb_write_string("Canceled. Returning to menu in 3 "
                                    "seconds.\n");
                    thd_sleep(3000);
                    last_buttons = 0;
                    goto restart_menu;
                }
                else if((buttons & (CONT_A | CONT_B)) == (CONT_A | CONT_B)) {
                    fb_write_string("Erasing PSO Serial Numbers...\n");

                    rv = erase_pso_keys();

                    if(rv < 0) {
                        fb_write_string("Error!\n"
                                        "Rebooting in 3 seconds...\n");
                        thd_sleep(3000);
                        arch_reboot();
                    }
                    else if(rv == 0) {
                        fb_write_string("No PSO Serial Numbers found.\n");
                    }
                    else {
                        fb_write_string("PSO Serial Numbers erased "
                                        "successfully\n");
                    }

                    fb_write_string("Returning to menu in 3 seconds.\n");
                    thd_sleep(3000);
                    last_buttons = 0;
                    goto restart_menu;
                }
            }

            rv = erase_pso_keys();

            if(rv < 0) {
                fb_write_string("Error!\n"
                                "Rebooting in 3 seconds...\n");
                thd_sleep(3000);
                arch_reboot();
            }
            else if(rv == 0) {
                fb_write_string("No PSO Serial Numbers Found!\n"
                                "Returning to menu in 3 seconds.\n");
                thd_sleep(3000);
                last_buttons = 0;
                goto restart_menu;
            }
            else {
                fb_write_string("Done!\n"
                                "Returning to menu in 3 seconds.\n");
                thd_sleep(3000);
                last_buttons = 0;
                goto restart_menu;
            }
        }
        else if((buttons & CONT_X) && !(last_buttons & CONT_X)) {
            fb_write_string("\n\n");
            fb_write_string("Are you sure you wish to erase your flashrom?\n"
                            "Press A + B to confirm, START to Cancel.\n"
                            "This cannot be undone!\n");

            for(;;) {
                buttons = wait_for_input();

                if((buttons & CONT_START)) {
                    fb_write_string("Canceled. Returning to menu in 3 "
                                    "seconds.\n");
                    thd_sleep(3000);
                    last_buttons = 0;
                    goto restart_menu;
                }
                else if((buttons & (CONT_A | CONT_B)) == (CONT_A | CONT_B)) {
                    fb_write_string("Erasing flashrom...\n");

                    if(erase_flashrom())
                        fb_write_string("Error!\n");
                    else
                        fb_write_string("Flashrom erased successfully\n");

                    fb_write_string("The console must now be rebooted...\n"
                                    "Rebooting in 3 seconds...\n");
                    thd_sleep(3000);
                    arch_reboot();
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    disclaimer();
    main_menu();
    return 0;
}
