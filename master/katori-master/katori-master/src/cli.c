#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "spoofing.h"
#include "sniffing.h"


static void print_welcome_header() {
    clear_window();

    fprintf(stdout, COLOR_YELLOW "/////////////////////////\n" COLOR_RESET);
    fprintf(stdout, COLOR_BLUE "   katori " COLOR_RESET "greets you!\n");
    fprintf(stdout, COLOR_YELLOW "\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\n" COLOR_RESET);
}


static void print_main_menu() {
    fprintf(stdout, "\n=================================\n");
    fprintf(stdout, "    [1] Sniffing mode.\n");
    fprintf(stdout, "    [2] Spoofing mode (isn't implemented yet).\n");
    fprintf(stdout, "    [3] Exit.\n");
    fprintf(stdout, "=================================\n");
}


int start_prompt() {
    strbuf_t *strbuf = create_strbuf(STRBUFSIZE);
    server_t *server = 0;

    if (strbuf == NULL) return strbuf_create_error;

    int err_code   = 0;
    int choice     = 0;
    int exit_flag  = 0;
    int first_run  = 1;

    print_welcome_header();    

    while (!exit_flag) {
        if (!first_run)
            clear_window();
        if (first_run) first_run = 0;
        
        print_main_menu();
        print_strbuf(strbuf);
        
        choice = input_choice();
        printf("\n");

        switch (choice) {
            case 1:
                sniffing_mode(&server, strbuf);
                break;
            case 2:
                spoofing_mode(strbuf);
                break;
            case 3:
                exit_flag = 1;
                break;
            default:
                bad(strbuf, "Invalid option.\n");
                break;
        }
    }

    info(strbuf, "Shutting down...\n");
    print_strbuf(strbuf);
    
    destroy_strbuf(strbuf);
    server_destroy(server);
    return err_code;
}
