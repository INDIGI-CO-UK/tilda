/*
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtk/gtk.h>
#include <vte/vte.h>
#include "tilda.h"
#include "../tilda-config.h"
#include "tilda_window.h"
#include "tilda_terminal.h"
#include "wizard.h"


void copy (gpointer data, guint callback_action, GtkWidget *w);
void paste (gpointer data, guint callback_action, GtkWidget *w);
void config_and_update (gpointer data, guint callback_action, GtkWidget *w);
void menu_quit (gpointer data, guint callback_action, GtkWidget *w);

static GtkItemFactoryEntry menu_items[] = {
    { "/_New Tab", "<Ctrl><Shift>T", add_tab_menu_call,     0, "<Item>",      NULL                  },
    { "/_Close Tab",      NULL,      close_tab,             0, "<Item>",      NULL                  },
    { "/sep1",            NULL,      NULL,                  0, "<Separator>", NULL                  },
    { "/_Copy",    "<Ctrl><Shift>C",      copy,                  0, "<Item>", NULL        },
    { "/_Paste",   "<Ctrl><Shift>V",      paste,                 0, "<Item>", NULL       },
    { "/sep1",            NULL,      NULL,                  0, "<Separator>", NULL                  },
    { "/_Preferences...", NULL,      config_and_update,     0, "<StockItem>", GTK_STOCK_PREFERENCES },
    { "/sep1",            NULL,      NULL,                  0, "<Separator>", NULL                  },
    { "/_Quit",         "<Ctrl>Q",   menu_quit,             0, "<StockItem>", GTK_STOCK_QUIT        }
};

static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

static void fix_size_settings (tilda_window *tw)
{
#ifdef DEBUG
    puts("fix_size_settings");
#endif

    int w, h;

    gtk_window_resize ((GtkWindow *) tw->window, cfg_getint (tw->tc, "max_width"), cfg_getint (tw->tc, "max_height"));
    gtk_window_get_size ((GtkWindow *) tw->window, &w, &h);
    cfg_setint (tw->tc, "max_width", w);
    cfg_setint (tw->tc, "max_height", h);

    gtk_window_resize ((GtkWindow *) tw->window, cfg_getint (tw->tc, "min_width"), cfg_getint (tw->tc, "min_height"));
    gtk_window_get_size ((GtkWindow *) tw->window, &w, &h);
    cfg_setint (tw->tc, "min_width", w);
    cfg_setint (tw->tc, "min_height", h);
}

void clean_up_no_args ()
{
#ifdef DEBUG
    puts("clean_up_no_args");
#endif

    gtk_main_quit ();
}

static void free_and_remove (tilda_window *tw)
{
#ifdef DEBUG
    puts("free_and_remove");
#endif

    guint i;

    /* Remove lock file */
    if (g_remove (tw->lock_file))
        fprintf (stderr, "Error removing lock file: %s\n", tw->lock_file);

    for (i=0; i<g_list_length(tw->terms); i++)
        g_free (g_list_nth_data (tw->terms, i));

    g_list_free (tw->terms);
}

void goto_tab (tilda_window *tw, guint i)
{
#ifdef DEBUG
    puts("goto_tab");
#endif

    gtk_notebook_set_current_page (GTK_NOTEBOOK (tw->notebook), i);
}

static gboolean goto_tab_generic (tilda_window *tw, gint tab_number)
{
#ifdef DEBUG
    printf ("goto_tab_%d\n", tab_number);
#endif

    if (g_list_length (tw->terms) > (tab_number-1))
    {
        goto_tab (tw, g_list_length (tw->terms) - tab_number);
        return TRUE;
    }

    return FALSE;
}

/* These all just call the generic function since they're all basically the same
 * anyway. Unfortunately, they can't just be macros, since we need to be able to
 * create a pointer to them for callbacks. */
gboolean goto_tab_1  (tilda_window *tw) { return goto_tab_generic (tw, 1);  }
gboolean goto_tab_2  (tilda_window *tw) { return goto_tab_generic (tw, 2);  }
gboolean goto_tab_3  (tilda_window *tw) { return goto_tab_generic (tw, 3);  }
gboolean goto_tab_4  (tilda_window *tw) { return goto_tab_generic (tw, 4);  }
gboolean goto_tab_5  (tilda_window *tw) { return goto_tab_generic (tw, 5);  }
gboolean goto_tab_6  (tilda_window *tw) { return goto_tab_generic (tw, 6);  }
gboolean goto_tab_7  (tilda_window *tw) { return goto_tab_generic (tw, 7);  }
gboolean goto_tab_8  (tilda_window *tw) { return goto_tab_generic (tw, 8);  }
gboolean goto_tab_9  (tilda_window *tw) { return goto_tab_generic (tw, 9);  }
gboolean goto_tab_10 (tilda_window *tw) { return goto_tab_generic (tw, 10); }

void next_tab (tilda_window *tw)
{
#ifdef DEBUG
    puts("next_tab");
#endif

    gtk_notebook_next_page (GTK_NOTEBOOK (tw->notebook));
}

void prev_tab (tilda_window *tw)
{
#ifdef DEBUG
    puts("prev_tab");
#endif

    gtk_notebook_prev_page ((GtkNotebook *) tw->notebook);
}

void clean_up (tilda_window *tw)
{
#ifdef DEBUG
    puts("clean_up");
#endif

    free_and_remove (tw);
    gtk_main_quit ();
    exit (0);
}

void start_program(tilda_collect *collect)
{
    int argc;
    char **argv;

    if (!cfg_getbool (collect->tw->tc, "run_command"))
    {
        /* Do nothing here, we'll get taken care of later */
    }
    else if (g_shell_parse_argv (cfg_getstr (collect->tw->tc, "command"), &argc, &argv, NULL))
    {
        vte_terminal_fork_command (VTE_TERMINAL(collect->tt->vte_term),
            argv[0], argv, NULL,
            cfg_getstr (collect->tw->tc, "working_dir"),
            TRUE, TRUE, TRUE);

        g_strfreev (argv);
        return; // the early way out
    }
    else /* An error in g_shell_parse_argv ??? */
    {
        perror ("tilda error");
        exit (1);
    }

    gchar *command = (gchar *) g_getenv ("SHELL");

    if (command == NULL)
        command = "/bin/sh";

    vte_terminal_fork_command (VTE_TERMINAL(collect->tt->vte_term),
        command, NULL, NULL,
        cfg_getstr (collect->tw->tc, "working_dir"),
        TRUE, TRUE, TRUE);
}

void close_tab_on_exit (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("close_tab_on_exit");
#endif

    tilda_collect *collect = (tilda_collect *) data;

    if (cfg_getbool (collect->tw->tc, "run_command"))
    {
        switch (cfg_getint (collect->tw->tc, "command_exit"))
        {
            case 2:
                close_tab (data, 0, widget);
                break;
            case 0:
                break;
            case 1:
                vte_terminal_feed (VTE_TERMINAL(collect->tt->vte_term), "\r\n\r\n", 4);

                start_program (collect);
                break;
            default:
                break;
        }
    }
    else
         close_tab (data, 0, widget);
}

char* get_window_title (GtkWidget *widget, tilda_window *tw)
{
#ifdef DEBUG
    puts("get_window_title");
#endif

    const gchar *vte_title;
    gchar *window_title;
    gchar *initial;
    gchar *title;

    vte_title = vte_terminal_get_window_title (VTE_TERMINAL (widget));
    window_title = g_strdup (vte_title);
    initial = g_strdup (cfg_getstr (tw->tc, "title"));

    switch (cfg_getint (tw->tc, "d_set_title"))
    {
        case 3:
            if (window_title != NULL)
                title = g_strdup (window_title);
            else
                title = g_strdup ("Untitled");
            break;

        case 2:
            if (window_title != NULL)
                title = g_strconcat (window_title, " - ", initial, NULL);
            else
                title = g_strdup (initial);
            break;

        case 1:
            if (window_title != NULL)
                title = g_strconcat (initial, " - ", window_title, NULL);
            else
                title = g_strdup (initial);
        break;

        case 0:
            title = g_strdup (initial);
            break;

        default:
            g_assert_not_reached ();
            title = NULL;
    }

    g_free (window_title);
    g_free (initial);

    return title;
}

void window_title_changed (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("window_title_changed");
#endif

    GtkWidget *page, *label;
    gchar *title;
    gint current_page_num;
    tilda_collect *tc = (tilda_collect *) data;
    tilda_window *tw = (tilda_window *) tc->tw;
    tilda_term *tt = (tilda_term *) tc->tt;

    title = get_window_title (widget, tw);

    label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (tw->notebook), tt->hbox);
    gtk_label_set_label ((GtkLabel *) label, title);
}

void deleted_and_quit (GtkWidget *widget, GdkEvent *event, gpointer data)
{
#ifdef DEBUG
    puts("deleted_and_quit");
#endif

    gtk_widget_destroy (GTK_WIDGET(data));
    gtk_main_quit();
}

void destroy_and_quit (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("destroy_and_quit");
#endif

    gtk_widget_destroy (GTK_WIDGET(data));
    gtk_main_quit ();
}

void destroy_and_quit_eof (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("destroy_and_quit_eof");
#endif

    close_tab_on_exit (widget, data);
}

void destroy_and_quit_exited (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("destroy_and_quit_exited");
#endif

    destroy_and_quit (widget, data);
}

void status_line_changed (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("status_line_changed");
#endif

    g_print ("Status = `%s'.\n", vte_terminal_get_status_line (VTE_TERMINAL(widget)));
}


void ccopy (tilda_window *tw)
{
#ifdef DEBUG
    puts("ccopy");
#endif
    GtkWidget *current_page;
    GList *list;

    gint pos = gtk_notebook_get_current_page (GTK_NOTEBOOK (tw->notebook));

    current_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (tw->notebook), pos);

    list = gtk_container_get_children ((GtkContainer *) current_page);

    vte_terminal_copy_clipboard ((VteTerminal *) list->data);
}

void cpaste (tilda_window *tw)
{
#ifdef DEBUG
    puts("cpaste");
#endif
    GtkWidget *current_page;
    GList *list;

    gint pos = gtk_notebook_get_current_page (GTK_NOTEBOOK (tw->notebook));

    current_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (tw->notebook), pos);

    list = gtk_container_get_children ((GtkContainer *) current_page);

    vte_terminal_paste_clipboard ((VteTerminal *) list->data);
}

void copy (gpointer data, guint callback_action, GtkWidget *w)
{
#ifdef DEBUG
    puts("copy");
#endif

    tilda_window *tw;
    tilda_term *tt;
    tilda_collect *tc = (tilda_collect *) data;

    tw = tc->tw;
    tt = tc->tt;

    vte_terminal_copy_clipboard ((VteTerminal *) tt->vte_term);
}

void paste (gpointer data, guint callback_action, GtkWidget *w)
{
#ifdef DEBUG
    puts("paste");
#endif

    tilda_window *tw;
    tilda_term *tt;
    tilda_collect *tc = (tilda_collect *) data;

    tw = tc->tw;
    tt = tc->tt;

    vte_terminal_paste_clipboard ((VteTerminal *) tt->vte_term);
}

void config_and_update (gpointer data, guint callback_action, GtkWidget *w)
{
#ifdef DEBUG
    puts("config_and_update");
#endif

    tilda_window *tw;
    tilda_term *tt;
    tilda_collect *tc = (tilda_collect *) data;

    tw = tc->tw;
    tt = tc->tt;

    wizard (-1, NULL, tw, tt);
}

void menu_quit (gpointer data, guint callback_action, GtkWidget *w)
{
#ifdef DEBUG
    puts("menu_quit");
#endif

    gtk_main_quit ();
}

void popup_menu (tilda_collect *tc)
{
#ifdef DEBUG
    puts("popup_menu");
#endif

    GtkItemFactory *item_factory;
    GtkWidget *menu;

    item_factory = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);

    gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, tc);

    menu = gtk_item_factory_get_widget (item_factory, "<main>");

    gtk_menu_popup (GTK_MENU(menu), NULL, NULL,
                   NULL, NULL, 3, gtk_get_current_event_time());

    gtk_widget_show_all(menu);
}

int add_tab_callback (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
#ifdef DEBUG
    puts("add_tab_callback");
#endif

    add_tab ((tilda_window *) data);
    return 0;
}

int button_pressed (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
#ifdef DEBUG
    puts("button_pressed");
#endif

    VteTerminal *terminal;
    tilda_term *tt;
    tilda_collect *tc;
    char *match;
    int tag;
    gint xpad, ypad;

    tc = (tilda_collect *) data;
    tt = tc->tt;

    switch (event->button)
    {
        case 3:
            popup_menu (tc);

            terminal  = VTE_TERMINAL(tt->vte_term);
            vte_terminal_get_padding (terminal, &xpad, &ypad);
            match = vte_terminal_match_check (terminal,
                (event->x - ypad) /
                terminal->char_width,
                (event->y - ypad) /
                terminal->char_height,
                &tag);
            if (match != NULL)
            {
                g_print ("Matched `%s' (%d).\n", match, tag);
                g_free (match);

                if (GPOINTER_TO_INT(data) != 0)
                    vte_terminal_match_remove (terminal, tag);
            }
            break;
        case 1:
        case 2:
        default:
            break;
    }

    return FALSE;
}

void iconify_window (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("iconify_window");
#endif

    if (GTK_IS_WIDGET(data))
        if ((GTK_WIDGET(data))->window)
            gdk_window_iconify ((GTK_WIDGET(data))->window);
}

void deiconify_window (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("deiconify_window");
#endif

    if (GTK_IS_WIDGET(data))
        if ((GTK_WIDGET(data))->window)
            gdk_window_deiconify ((GTK_WIDGET(data))->window);
}

void raise_window (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("raise_window");
#endif

    if (GTK_IS_WIDGET(data))
        if ((GTK_WIDGET(data))->window)
            gdk_window_raise ((GTK_WIDGET(data))->window);
}

void lower_window (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("lower_window");
#endif

    if (GTK_IS_WIDGET(data))
        if ((GTK_WIDGET(data))->window)
            gdk_window_lower ((GTK_WIDGET(data))->window);
}

void maximize_window (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("maximize_window");
#endif

    if (GTK_IS_WIDGET(data))
        if ((GTK_WIDGET(data))->window)
            gdk_window_maximize ((GTK_WIDGET(data))->window);
}

void restore_window (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("restore_window");
#endif

    if (GTK_IS_WIDGET(data))
        if ((GTK_WIDGET(data))->window)
            gdk_window_unmaximize ((GTK_WIDGET(data))->window);
}

void refresh_window (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("refresh_window");
#endif

    GdkRectangle rect;
    if (GTK_IS_WIDGET(data))
    {
        if ((GTK_WIDGET(data))->window)
        {
            rect.x = rect.y = 0;
            rect.width = (GTK_WIDGET(data))->allocation.width;
            rect.height = (GTK_WIDGET(data))->allocation.height;
            gdk_window_invalidate_rect ((GTK_WIDGET(data))->window,
                           &rect, TRUE);
        }
    }
}

void resize_window (GtkWidget *widget, guint width, guint height, gpointer data)
{
#ifdef DEBUG
    puts("resize_window");
#endif

    VteTerminal *terminal;
    gint owidth, oheight, xpad, ypad;

    if ((GTK_IS_WINDOW(data)) && (width >= 2) && (height >= 2))
    {
        terminal = VTE_TERMINAL(widget);

        /* Take into account border overhead. */
        gtk_window_get_size (GTK_WINDOW(data), &owidth, &oheight);
        owidth -= terminal->char_width * terminal->column_count;
        oheight -= terminal->char_height * terminal->row_count;

        /* Take into account padding, which needn't be re-added. */
        vte_terminal_get_padding (VTE_TERMINAL(widget), &xpad, &ypad);
        owidth -= xpad;
        oheight -= ypad;
        gtk_window_resize (GTK_WINDOW(data), width + owidth, height + oheight);
    }
}

void move_window (GtkWidget *widget, guint x, guint y, gpointer data)
{
#ifdef DEBUG
    puts("move_window");
#endif

    if (GTK_IS_WIDGET(data))
        if ((GTK_WIDGET(data))->window)
            gdk_window_move ((GTK_WIDGET(data))->window, x, y);
}
void focus_term (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("focus_term");
#endif

    GList *list;
    GtkWidget *box;
    GtkWidget *n = (GtkWidget *) data;

    box = gtk_notebook_get_nth_page ((GtkNotebook *) n, gtk_notebook_get_current_page((GtkNotebook *) n));
    list = gtk_container_children ((GtkContainer *) box);
    gtk_widget_grab_focus (list->data);
}

void adjust_font_size (GtkWidget *widget, gpointer data, gint howmuch)
{
#ifdef DEBUG
    puts("adjust_font_size");
#endif

    VteTerminal *terminal;
    PangoFontDescription *desired;
    gint newsize;
    gint columns, rows, owidth, oheight;

    /* Read the screen dimensions in cells. */
    terminal = VTE_TERMINAL(widget);
    columns = terminal->column_count;
    rows = terminal->row_count;

    /* Take into account padding and border overhead. */
    gtk_window_get_size(GTK_WINDOW(data), &owidth, &oheight);
    owidth -= terminal->char_width * terminal->column_count;
    oheight -= terminal->char_height * terminal->row_count;

    /* Calculate the new font size. */
    desired = pango_font_description_copy (vte_terminal_get_font(terminal));
    newsize = pango_font_description_get_size (desired) / PANGO_SCALE;
    newsize += howmuch;
    pango_font_description_set_size (desired, CLAMP(newsize, 4, 144) * PANGO_SCALE);

    /* Change the font, then resize the window so that we have the same
     * number of rows and columns. */
    vte_terminal_set_font (terminal, desired);
    /*gtk_window_resize (GTK_WINDOW(data),
              columns * terminal->char_width + owidth,
              rows * terminal->char_height + oheight);*/

    pango_font_description_free (desired);
}

void increase_font_size(GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("increase_font_size");
#endif

    adjust_font_size (widget, data, 1);
}

void decrease_font_size (GtkWidget *widget, gpointer data)
{
#ifdef DEBUG
    puts("decrease_font_size");
#endif

    adjust_font_size (widget, data, -1);
}

