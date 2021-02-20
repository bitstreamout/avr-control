/* Denon AVR-3310 Control Utility 0.01
   Copyright (C) 2012 James Turner

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gio/gunixinputstream.h>
#include <json-glib/json-glib.h>
#include <json-glib/json-gobject.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


static void sigchld(int sig __attribute__((__unused__)))
{
	const int old_errno = errno;
	int status;
	pid_t pid;

	while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) != 0) {
		if (errno == ECHILD)
			break;
		if (pid < 0)
			break;
	}
	errno = old_errno;
}

// Runs the specified command as an asynchronous child process

void run_command_async(char *command)
{
	pid_t pid;

	if((pid = fork())==0)
	{
		system(command);
		exit(0);
	}
}

// Signal handler for operating the power switch

void power_state_change(GObject *obj,GParamSpec *pspec,gpointer *data)
{
	int power_state;

	g_object_get(obj,"active",&power_state,NULL);

	if(power_state == 1)
		run_command_async("avr --power ON");
	else
		run_command_async("avr --power STANDBY");
}

// Signal handler for operating the input selector

void input_select(GtkComboBox *input_selector,gpointer user_data)
{
	char command[50];
	sprintf(command,"avr --input %s",gtk_combo_box_get_active_id(input_selector));
	run_command_async(command);
}

// Signal handler for operating the volume control

void volume_change(GtkWidget *volume_slider,gpointer user_data)
{
	char command[50];
	sprintf(command,"avr --volume %d",(int)gtk_range_get_value((GtkRange *)volume_slider));
	run_command_async(command);
}

// Signal handler to quit application when main window is closed

void window_close(GtkWidget *widget)
{
	gtk_main_quit();
}

// Populate input selector with sources (TODO: ideally these would be read from the device)

void populate_input_list(GtkComboBoxText *input_selector)
{
	// generic setup
#if 0
	gtk_combo_box_text_append(input_selector,"TUNER",	"Tuner");
	gtk_combo_box_text_append(input_selector,"Phono",	"Phono");
	gtk_combo_box_text_append(input_selector,"CD",		"CD");
	gtk_combo_box_text_append(input_selector,"DVD",		"DVD");
	gtk_combo_box_text_append(input_selector,"HDP",		"HDP");
	gtk_combo_box_text_append(input_selector,"TV",		"TV");
	gtk_combo_box_text_append(input_selector,"SAT/CBL",	"SAT/CBL");
	gtk_combo_box_text_append(input_selector,"VCR",		"VCR");
	gtk_combo_box_text_append(input_selector,"DVR",		"DVR");
	gtk_combo_box_text_append(input_selector,"V.AUX",	"V.AUX");
	gtk_combo_box_text_append(input_selector,"NET/USB",	"NET/USB");
	gtk_combo_box_text_append(input_selector,"XM",		"XM");
	gtk_combo_box_text_append(input_selector,"SIRIUS",	"SIRIUS");
	gtk_combo_box_text_append(input_selector,"HDRADIO",	"HDRADIO");
#endif
	// my setup
	// lwp-request http://192.168.0.50/goform/formMainZone_MainZoneXmlStatus.xml
	gtk_combo_box_text_append(input_selector,"SAT/CBL",	"SAT-PVR");
//	gtk_combo_box_text_append(input_selector,"DVD",		"DVD");
	gtk_combo_box_text_append(input_selector,"BD",		"Oppo");
//	gtk_combo_box_text_append(input_selector,"GAME",	"Game");
	gtk_combo_box_text_append(input_selector,"AUX1",	"Front");
	gtk_combo_box_text_append(input_selector,"AUX2",	"Tuner");
	gtk_combo_box_text_append(input_selector,"MPLAY",	"Mediaplayer");
	gtk_combo_box_text_append(input_selector,"GAME",	"Chromecast");
	gtk_combo_box_text_append(input_selector,"iPod/USB",	"iPod/USB");
	gtk_combo_box_text_append(input_selector,"CD",		"CD oppo");
//	gtk_combo_box_text_append(input_selector,"TUNER",	"Tuner");
	gtk_combo_box_text_append(input_selector,"SERVER",	"Online Music");
	gtk_combo_box_text_append(input_selector,"TV AUDIO",	"TV ARC");
	gtk_combo_box_text_append(input_selector,"Bluetooth",	"Bluetooth");
//	gtk_combo_box_text_append(input_selector,"PHONO",	"Phono");
}


void populate_input_alist(JsonArray *array, guint index, JsonNode *node, gpointer data)
{
	GtkComboBoxText *input_selector = (GtkComboBoxText *)data;
	JsonObject *object;
	const char *v, *d, *n;

	(void)array;
	(void)index;
	object = json_node_get_object(node);

	if (!(v = (char*)json_object_get_string_member(object, "Visible")) || strcmp(v, "1") != 0)
		return;
	d = json_object_get_string_member(object, "Device");
	n = json_object_get_string_member(object, "Name");

	if (!d || !n)
		return;

	gtk_combo_box_text_append(input_selector, d, n);
}

// Driver function

int main (int argc, char *argv[])
{
	GError* error = NULL;
        GtkBuilder *builder;
        GObject *main_window;
        GObject *volume_slider;
        GObject *input_selector;
	GObject *power_switch;
	JsonParser *parser = NULL;
	JsonNode *root = NULL;
	JsonArray *array = NULL;
	struct sigaction sa;
	const char *HOME = getenv("HOME");
	char *config = NULL;
	char *input = NULL;

	if (asprintf(&config, "%s/.config/avr-control/avr-control.xml", HOME) < 0) {
		perror("Can not open avr-control.xml");
		goto err;
	}
	if (asprintf(&input, "%s/.config/avr-control/avr-input.json", HOME) < 0) {
		perror("Can not open avr-input.json");
		goto err;
	}

        gtk_init(&argc,&argv);

	if (!(parser = json_parser_new())) {
		perror("Can not open avr-input.json");
		goto err;
		exit(1);
	}
	if (!json_parser_load_from_file(parser, input, &error)) {
		g_error("Can not open avr-input.json: %s", error->message);
		goto err;
	}
	if ((root = json_parser_get_root(parser)) == NULL) {
		g_error("Can not open avr-input.json: %s", error->message);
		goto err;
	}

	if (JSON_NODE_TYPE (root) != JSON_NODE_ARRAY) {
		perror("The avr-input.json is not an JSON array");
		goto err;
	}

	if (!(array = json_node_get_array (root)) || json_array_get_length(array) == 0) {
		perror("The array in avr-input.json is empty");
		goto err;
	}

        builder = gtk_builder_new();
        gtk_builder_add_from_file(builder,config,NULL);
	if (!gtk_builder_add_from_file (builder, config, &error)){
		g_error("Couldn't load builder file: %s", error->message);
		goto err;
	}
	free(config);
	config = NULL;

	volume_slider = gtk_builder_get_object(builder,"volume_slider");
	gtk_widget_set_focus_on_click(GTK_WIDGET(volume_slider), FALSE);

	g_signal_connect(volume_slider,"value-changed",G_CALLBACK(volume_change),NULL);

	input_selector = gtk_builder_get_object(builder,"input_selector");

	json_array_foreach_element(array, populate_input_alist, (gpointer)(GTK_COMBO_BOX_TEXT(input_selector)));
	g_object_unref(parser);

	g_signal_connect(input_selector,"changed",G_CALLBACK(input_select),NULL);

	// Glade doesn't support connecting notify:active signal handler so do it here instead
	power_switch = gtk_builder_get_object(builder,"power_switch");
	g_signal_connect(power_switch,"notify::active",G_CALLBACK(power_state_change),NULL);

        main_window = gtk_builder_get_object(builder,"main_window");
	g_signal_connect(main_window,"delete-event",G_CALLBACK(window_close),NULL);
        gtk_widget_show(GTK_WIDGET(main_window));

	daemon(0,0);

	sa.sa_handler = sigchld;
	sa.sa_flags = SA_RESTART;
	(void)sigemptyset(&sa.sa_mask);
	(void)sigaddset(&sa.sa_mask, SIGCHLD);
	sigaction(SIGCHLD, &sa, 0);

        gtk_main();

        return 0;
err:
	if (config)
		free(config);
	if (input)
		free(input);
	if (error)
		g_error_free(error);
	if (parser)
		g_object_unref(parser);
	return 1;
}
