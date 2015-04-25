
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-cursor.h>
#include "os-compatibility.h"

struct cursor_viewer {
    struct wl_display       *display;
    struct wl_registry      *registry;
    struct wl_compositor    *compositor;
    struct wl_shm           *shm;
    struct wl_shell         *shell;
    struct wl_cursor_theme  *cursor_theme;
};

struct cursor_surface {
    const char *name;
    struct cursor_viewer    *viewer;
    struct wl_surface       *surface;
    struct wl_shell_surface *shsurf;
    struct wl_cursor        *cursor;
    struct wl_callback      *frame;
};

static sig_atomic_t running = 1;

static void
die(const char msg[])
{
    fprintf(stderr, "%s", msg);
    exit(EXIT_FAILURE);
}

static void
registry_handle_global(
    void *data, struct wl_registry *registry, uint32_t name,
    const char *interface, uint32_t version)
{
    struct cursor_viewer *viewer = data;
    if (strcmp(interface, "wl_compositor") == 0)
        viewer->compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    else if (strcmp(interface, "wl_shell") == 0)
        viewer->shell =
            wl_registry_bind(registry, name, &wl_shell_interface, 1);
    else if (strcmp(interface, "wl_shm") == 0)
        viewer->shm =
            wl_registry_bind(registry, name, &wl_shm_interface, 1);
}

static void
registry_handle_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name)
{
}

static struct wl_registry_listener registry_listener = {
    registry_handle_global, registry_handle_global_remove
};

static void
shsurf_handle_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void
shsurf_handle_configure(void *data,
    struct wl_shell_surface *wl_shell_surface, uint32_t edges,
    int32_t width, int32_t height)
{
}

static void
shsurf_handle_popup_done(void *data,
    struct wl_shell_surface *wl_shell_surface)
{
}

static const struct wl_shell_surface_listener shsurf_listener = {
    shsurf_handle_ping, shsurf_handle_configure, shsurf_handle_popup_done
};

static void surface_frame_done(void *data, struct wl_callback *wl_callback, uint32_t time);

static const struct wl_callback_listener frame_listener = {
    &surface_frame_done
};

static void
surface_frame_done(void *data, struct wl_callback *wl_callback, uint32_t time)
{
    struct wl_cursor_image *cursor_image;
    struct wl_buffer *cursor_buffer = NULL;
    struct cursor_surface *cursor = (struct cursor_surface *)data;
    int image_index = wl_cursor_frame(cursor->cursor, time);

    cursor_image = cursor->cursor->images[image_index];
    cursor_buffer = wl_cursor_image_get_buffer(cursor_image);

    if (cursor->frame)
        wl_callback_destroy(cursor->frame);
    cursor->frame = wl_surface_frame(cursor->surface);
    wl_callback_add_listener(cursor->frame, &frame_listener, cursor);
    wl_surface_attach(cursor->surface, cursor_buffer, 0, 0);
    wl_surface_damage(cursor->surface, 0, 0, cursor_image->width, cursor_image->height);
    wl_surface_commit(cursor->surface);
}

static void
cursor_viewer_destroy(struct cursor_viewer *viewer)
{
    if (viewer->cursor_theme)
        wl_cursor_theme_destroy(viewer->cursor_theme);
    if (viewer->shell)
        wl_shell_destroy(viewer->shell);
    if (viewer->compositor)
        wl_compositor_destroy(viewer->compositor);
    if (viewer->shm)
        wl_shm_destroy(viewer->shm);
    if (viewer->registry)
        wl_registry_destroy(viewer->registry);
    if (viewer->display)
        wl_display_disconnect(viewer->display);
    free(viewer);
}

static struct cursor_viewer *
cursor_viewer_create(void)
{
    struct cursor_viewer *viewer = calloc(1, sizeof (struct cursor_viewer));
    if (!viewer)
        die("Cannot allocate memory for cursor_viewer\n");

    viewer->display = wl_display_connect(NULL);
    if (!viewer->display) {
        cursor_viewer_destroy(viewer);
        die("Cannot connect to Wayland display\n");
    }

    viewer->registry = wl_display_get_registry(viewer->display);
    if (!viewer->registry) {
        cursor_viewer_destroy(viewer);
        die("Cannot get registry from Wayland display\n");
    }
    wl_registry_add_listener(viewer->registry, &registry_listener, viewer);

    wl_display_roundtrip(viewer->display);
    wl_display_roundtrip(viewer->display);

    return viewer;
}

static void
cursor_surface_destroy(struct cursor_surface *cursor)
{
    if (cursor->shsurf)
        wl_shell_surface_destroy(cursor->shsurf);
    if (cursor->surface)
        wl_surface_destroy(cursor->surface);
    if (cursor->frame)
        wl_callback_destroy(cursor->frame);
    free(cursor);
}

static struct cursor_surface *
cursor_viewer_show_cursor(struct cursor_viewer *viewer, const char *name)
{
    struct wl_cursor_image *cursor_image;
    struct wl_buffer *cursor_buffer = NULL;

    struct cursor_surface *cursor = calloc(1, sizeof (struct cursor_surface));
    if (!cursor)
        die("Cannot allocate memory for cursor_surface\n");

    cursor->name = name;
    cursor->surface = wl_compositor_create_surface(viewer->compositor);
    cursor->shsurf = wl_shell_get_shell_surface(viewer->shell, cursor->surface);

    wl_shell_surface_add_listener(cursor->shsurf, &shsurf_listener, viewer);
    wl_shell_surface_set_toplevel(cursor->shsurf);
    wl_shell_surface_set_title(cursor->shsurf, name);
    wl_surface_set_user_data(cursor->surface, viewer);

    cursor->cursor = wl_cursor_theme_get_cursor(viewer->cursor_theme, name);
    if (!cursor->cursor || cursor->cursor->image_count < 1) {
        printf("No such cursor: %s\n", name);
        cursor_surface_destroy(cursor);
        return NULL;
    }

    cursor_image = cursor->cursor->images[0];
    cursor_buffer = wl_cursor_image_get_buffer(cursor_image);

    printf("%s: %s: size=%dx%d image_count=%u\n",
        __func__, cursor->name,
        cursor_image->width, cursor_image->height,
        cursor->cursor->image_count);

    if (cursor->cursor->image_count > 1) {
        cursor->frame = wl_surface_frame(cursor->surface);
        wl_callback_add_listener(cursor->frame, &frame_listener, cursor);
    }

    wl_surface_attach(cursor->surface, cursor_buffer, 0, 0);
    wl_surface_damage(cursor->surface, 0, 0, cursor_image->width, cursor_image->height);
    wl_surface_commit(cursor->surface);

    return cursor;
}

static void
signal_int(int signum)
{
    running = 0;
}

int
main(int argc, char **argv)
{
    int i, ret = 0, cursor_count = argc - 3;
    struct sigaction sigint;
    struct cursor_viewer *viewer;
    struct cursor_surface **cursors;

    if (argc < 4) {
        printf("Usage: %s CURSOR_THEME SIZE CURSOR_NAMES...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    sigint.sa_handler = signal_int;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sigint, NULL);

    viewer = cursor_viewer_create();

    viewer->cursor_theme =
        wl_cursor_theme_load(argv[1], atoi(argv[2]), viewer->shm);
    if (!viewer->cursor_theme)
        die("Failed to load default cursor theme\n");

    cursors = calloc(1, sizeof (struct cursor_surface *) * cursor_count);

    for (i = 3; i < argc; ++i) {
        struct cursor_surface *cursor =
            cursor_viewer_show_cursor(viewer, argv[i]);

        cursors[i - 3] = cursor;
    }

    while (running && ret != -1)
        ret = wl_display_dispatch(viewer->display);

    for (i = 0; i < cursor_count; ++i)
        cursor_surface_destroy(cursors[i]);
    free(cursors);

    cursor_viewer_destroy(viewer);

    exit(EXIT_SUCCESS);
}
