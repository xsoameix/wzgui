#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <float.h>
#include <gtk/gtk.h>
#include <wz/unicode.h>
#include <wz/file.h>
#include "var_grid.h"
#include "file_tree_store.h"

void
window_on_destroy(GtkWidget * window, gpointer userdata) {
  int * exit = userdata;
  * exit = 1;
}

int
read_file(wzfile * file, FILE * raw, wzctx * ctx) {
  file->raw = raw, file->pos = 0;
  if (fseek(raw, 0, SEEK_END)) return 1;
  long size = ftell(raw);
  if (size < 0) return 1;
  file->size = (uint32_t) size;
  rewind(raw);
  if (wz_read_head(&file->head, file) ||
      wz_read_le16(&file->ver.enc, file)) return 1;
  if (wz_deduce_ver(&file->ver, file, ctx))
    return wz_free_head(&file->head), 1;
  return 0;
}

int
open_file(wzfile * file, char * filename, wzctx * ctx) {
  file->raw = fopen(filename, "rb");
  if (file->raw == NULL) return perror(filename), 1;
  if (read_file(file, file->raw, ctx)) return fclose(file->raw) != 0;
  return 0;
}

enum {
  COL_VAR_NAME,
  COL_VAR_TYPE,
  COL_VAR_PRIM_TYPE,
  COL_VAR_OBJ_TYPE,
  COL_VAR_DATA,
  COL_VAR_LEN
};

void
var_grid_destroy(GtkWidget * vgrid) {
  var_detail detail = var_grid_get_detail(VAR_GRID(vgrid));
  wzvar * var = * detail.var;
  if (WZ_IS_VAR_INT16(var->type) ||
      WZ_IS_VAR_INT32(var->type) ||
      WZ_IS_VAR_INT64(var->type)) {
    var_int_detail * int_detail = detail.i;
    free(int_detail->val);
    free(int_detail);
  } else if (WZ_IS_VAR_FLOAT32(var->type) ||
             WZ_IS_VAR_FLOAT64(var->type)) {
    var_flt_detail * flt_detail = detail.f;
    free(flt_detail->val);
    free(flt_detail);
  } else if (WZ_IS_VAR_STRING(var->type)) {
    wzchr * val = &var->val.str;
    var_str_detail * str_detail = detail.str;
    if (val->enc == WZ_ENC_UTF16LE)
      free(str_detail->bytes);
    free(str_detail);
  } else if (WZ_IS_VAR_OBJECT(var->type)) {
    wzobj * obj = var->val.obj;
    if (WZ_IS_OBJ_PROPERTY(&obj->type)) {
      var_list_detail * list_detail = detail.list;
      free(list_detail->len);
      free(list_detail);
    } else if (WZ_IS_OBJ_CANVAS(&obj->type)) {
      var_img_detail * img_detail = detail.img;
      free(img_detail->data);
      free(img_detail->h);
      free(img_detail->w);
      free(img_detail->len);
      free(img_detail);
    } else if (WZ_IS_OBJ_CONVEX(&obj->type)) {
      var_vex_detail * vex_detail = detail.vex;
      wzvex * vex = (wzvex *) obj;
      for (uint32_t i = 0; i < vex->len; i++) {
        free(vex_detail->vals[i].x);
        free(vex_detail->vals[i].y);
      }
      free(vex_detail->vals);
      free(vex_detail->len);
      free(vex_detail);
    } else if (WZ_IS_OBJ_VECTOR(&obj->type)) {
      var_vec_detail * vec_detail = detail.vec;
      free(vec_detail->val.x);
      free(vec_detail->val.y);
      free(vec_detail);
    } else if (WZ_IS_OBJ_SOUND(&obj->type)) {
      var_ao_detail * ao_detail = detail.ao;
      wzao * ao = (wzao *) obj;
      if (ao->format == WZ_AUDIO_PCM) {
        if (Pa_CloseStream(ao_detail->stream) != paNoError)
          printf("failed to close the PortAudio stream\n");
        free(ao_detail->size);
        free(ao_detail->ms);
        free(ao_detail);
      } else if (ao->format == WZ_AUDIO_MP3) {
        g_rec_mutex_lock(&ao_detail->mutex);
        ao_detail->close = 1;
        g_rec_mutex_unlock(&ao_detail->mutex);
        g_thread_join(ao_detail->thread);
        g_rec_mutex_clear(&ao_detail->mutex);
        if (ao_detail->idle_id)
          g_source_remove(ao_detail->idle_id);
        free(ao_detail->size);
        free(ao_detail->ms);
        free(ao_detail);
      }
    } else if (WZ_IS_OBJ_UOL(&obj->type)) {
      var_uol_detail * uol_detail = detail.uol;
      free(uol_detail);
    }
  }
  char * type_detail = var_grid_get_type_detail(VAR_GRID(vgrid));
  free(type_detail);
  gtk_widget_destroy(vgrid);
}

GtkWidget *
var_grid_insert(GtkWidget * vgrid, char * name) {
  GtkWidget * label = gtk_label_new(name);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_show(label);
  gtk_grid_attach_next_to(GTK_GRID(vgrid), label, NULL, GTK_POS_BOTTOM, 1, 1);

  PangoAttrList * struct_attr_list = pango_attr_list_new();
  PangoAttribute * struct_attr_weight =
      pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  pango_attr_list_insert(struct_attr_list, struct_attr_weight);
  gtk_label_set_attributes(GTK_LABEL(label), struct_attr_list);

  GtkWidget * grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
  gtk_widget_set_margin_top(grid, 6);
  gtk_widget_set_margin_left(grid, 12);
  gtk_widget_set_margin_bottom(grid, 6);
  gtk_widget_show(grid);
  gtk_grid_attach_next_to(GTK_GRID(vgrid), grid, NULL, GTK_POS_BOTTOM, 1, 1);

  return grid;
}

int
var_grid_struct_insert(GtkWidget * grid_struct, char * name,
                       char * type_bytes, int row) {
  int col = 0;

  GtkWidget * label_val = gtk_label_new(name);
  gtk_widget_set_valign(label_val, GTK_ALIGN_START);
  gtk_widget_set_halign(label_val, GTK_ALIGN_START);
  gtk_widget_set_margin_top(label_val, 6);
  gtk_widget_show(label_val);
  gtk_grid_attach(GTK_GRID(grid_struct), label_val, col++, row, 1, 1);

  GtkWidget * label_val_type = gtk_label_new(type_bytes);
  gtk_widget_set_valign(label_val_type, GTK_ALIGN_START);
  gtk_widget_set_halign(label_val_type, GTK_ALIGN_START);
  gtk_widget_set_margin_top(label_val_type, 6);
  gtk_widget_show(label_val_type);
  gtk_grid_attach(GTK_GRID(grid_struct), label_val_type, col++, row, 1, 1);

  PangoAttrList * val_type_attr_list = pango_attr_list_new();
  PangoAttribute * val_type_attr_style =
      pango_attr_style_new(PANGO_STYLE_ITALIC);
  pango_attr_list_insert(val_type_attr_list, val_type_attr_style);
  gtk_label_set_attributes(GTK_LABEL(label_val_type), val_type_attr_list);

  return col;
}

int
var_grid_struct_insert_value(GtkWidget * grid_struct,
                             char * type_bytes, int row) {
  return var_grid_struct_insert(grid_struct, "value", type_bytes, row);
}

void
var_grid_struct_insert_entry(GtkWidget * grid_struct,
                             char * val, int row, int col) {
  GtkWidget * entry_val_value = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_val_value), val);
  gtk_editable_set_editable(GTK_EDITABLE(entry_val_value), FALSE);
  gtk_widget_show(entry_val_value);
  gtk_grid_attach(GTK_GRID(grid_struct), entry_val_value, col, row, 1, 1);
}

void
var_grid_struct_insert_label(GtkWidget * grid_struct, char * name,
                             int row, int col) {
  GtkWidget * label_val = gtk_label_new(name);
  gtk_widget_set_margin_top(label_val, 3);
  gtk_widget_set_margin_left(label_val, 6);
  gtk_widget_set_margin_bottom(label_val, 6);
  gtk_widget_set_halign(label_val, GTK_ALIGN_START);
  gtk_widget_show(label_val);
  gtk_grid_attach(GTK_GRID(grid_struct), label_val, col, row, 1, 1);
}

enum {
  COL_VEC_X,
  COL_VEC_Y,
  COL_VEC_LEN
};

GtkTreeStore *
obj_grid_insert_vex(GtkWidget * obj_grid, char * type_bytes,
                    wz2d * vals, uint32_t len, int obj_row) {
  GtkAdjustment * tree_view_vex_window_hadjustment =
      gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  GtkAdjustment * tree_view_vex_window_vadjustment =
      gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  GtkWidget * tree_view_vex_window = gtk_scrolled_window_new(
      tree_view_vex_window_hadjustment, tree_view_vex_window_vadjustment);
  gtk_widget_show(tree_view_vex_window);

  int obj_col = var_grid_struct_insert(obj_grid, "value", type_bytes, obj_row);
  gtk_grid_attach(GTK_GRID(obj_grid), tree_view_vex_window,
                  obj_col, obj_row++, 1, 1);

  GtkWidget * tree_view_vex = gtk_tree_view_new();
  gtk_widget_set_vexpand(tree_view_vex, TRUE);
  gtk_widget_show(tree_view_vex);
  gtk_container_add(GTK_CONTAINER(tree_view_vex_window), tree_view_vex);

  GtkCellRenderer * cell_renderer_text = gtk_cell_renderer_text_new();
  gtk_cell_renderer_set_alignment(cell_renderer_text, 1.0, 0.5);

  GtkTreeViewColumn * column_x = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column_x, "x");
  gtk_tree_view_column_set_alignment(column_x, 0.5);
  gtk_tree_view_column_set_expand(column_x, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_vex), column_x);

  gtk_tree_view_column_pack_start(column_x, cell_renderer_text, TRUE);
  gtk_tree_view_column_set_attributes(column_x, cell_renderer_text,
                                      "text", COL_VEC_X, NULL);

  GtkTreeViewColumn * column_y = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column_y, "y");
  gtk_tree_view_column_set_alignment(column_y, 0.5);
  gtk_tree_view_column_set_expand(column_y, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_vex), column_y);

  gtk_tree_view_column_pack_start(column_y, cell_renderer_text, TRUE);
  gtk_tree_view_column_set_attributes(column_y, cell_renderer_text,
                                      "text", COL_VEC_Y, NULL);

  GtkTreeStore * tree_store = gtk_tree_store_new(
      COL_VEC_LEN, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_vex),
                          GTK_TREE_MODEL(tree_store));

  return tree_store;
}

int
pcm_cb(const void * in, void * out, unsigned long frame_count,
       const PaStreamCallbackTimeInfo * time_info,
       PaStreamCallbackFlags flags, void * userdata) {
  var_ao_detail * ao_detail = userdata;
  unsigned long read_size = frame_count * ao_detail->frame_size;
  uint32_t avail_size = ao_detail->data_size - ao_detail->data_read;
  if (read_size <= avail_size) {
    memcpy(out, ao_detail->data + ao_detail->data_read, read_size);
    ao_detail->data_read += read_size;
    return paContinue;
  } else {
    memcpy(out, ao_detail->data + ao_detail->data_read, avail_size);
    memset(out + avail_size, 0, read_size - avail_size);
    return paComplete;
  }
}

gchar *
scale_on_format_value(GtkScale * scale, gdouble value, gpointer userdata) {
  var_ao_detail * ao_detail = userdata;
  wzao * ao = (wzao *) ao_detail->var->val.obj;
  gdouble upper = gtk_adjustment_get_upper(ao_detail->adjustment);
  uint32_t s = (uint32_t) (value / upper * ao->ms / 1000);
  uint32_t len = ao->ms / 1000;
  return g_strdup_printf("%"PRIu32":%02"PRIu32" / %"PRIu32":%02"PRIu32,
                         s / 60, s % 60, len / 60, len % 60);
}

gboolean
scale_on_button_press_event(GtkWidget * widget, GdkEvent * event,
                            gpointer userdata) {
  var_ao_detail * ao_detail = userdata;
  ao_detail->draged_begin = 1;
  return FALSE;
}

gboolean
scale_on_button_release_event(GtkWidget * widget, GdkEvent * event,
                              gpointer userdata) {
  GtkAdjustment * adjustment = gtk_range_get_adjustment(GTK_RANGE(widget));
  gdouble changed = gtk_adjustment_get_value(adjustment);

  var_ao_detail * ao_detail = userdata;
  ao_detail->draged_begin = 0;
  g_rec_mutex_lock(&ao_detail->mutex);
  ao_detail->draged_end = 1, ao_detail->changed = changed;
  g_rec_mutex_unlock(&ao_detail->mutex);
  return FALSE;
}

gboolean
adjustment_conifigure(gpointer userdata) {
  var_ao_detail * ao_detail = userdata;
  gtk_adjustment_configure(ao_detail->adjustment,
                           0, 0, ao_detail->len, 1.0, 1.0, 1.0);
  return FALSE;
}

gboolean
adjustment_set_value(gpointer userdata) {
  var_ao_detail * ao_detail = userdata;
  if (!ao_detail->draged_begin)
    gtk_adjustment_set_value(ao_detail->adjustment, ao_detail->now);
  ao_detail->idle_id = 0;
  return FALSE;
}

void
button_on_toggled(GtkToggleButton * button, gpointer userdata) {
  var_ao_detail * ao_detail = userdata;
  g_rec_mutex_lock(&ao_detail->mutex);
  ao_detail->repeat = gtk_toggle_button_get_active(button);
  g_rec_mutex_unlock(&ao_detail->mutex);
  GtkStyleContext * style_context =
      gtk_widget_get_style_context(GTK_WIDGET(button));
  if (ao_detail->repeat)
    gtk_style_context_remove_class(style_context, "clean");
  else
    gtk_style_context_add_class(style_context, "clean");
}

gpointer
thread_play_mp3(gpointer userdata) {
  var_ao_detail * ao_detail = userdata;
  int err;
  mpg123_handle * handle = mpg123_new(NULL, &err);
  if (handle == NULL)
    return printf("failed to malloc\n"), NULL;
  if (mpg123_open_feed(handle) != MPG123_OK)
    return printf("failed to open feed of mp3 data\n"), NULL;
  if (mpg123_feed(handle, ao_detail->data, ao_detail->data_size) != MPG123_OK)
    return printf("failed to feed mp3 data\n"), NULL;
  off_t frame_offset;
  size_t size;
  unsigned char * data;
  long sample_rate;
  int channels;
  int enc;

  int format;
  int formats = 0;
  off_t len = 0;
  for (int quit = 0; !quit;) {
    switch (mpg123_decode_frame(handle, &frame_offset, &data, &size)) {
    case MPG123_NEW_FORMAT:
      if (mpg123_getformat(handle, &sample_rate, &channels, &enc) != MPG123_OK)
        return printf("failed to get format of mp3 header\n"), NULL;
      format = mpg123_encsize(enc), formats++;
    case MPG123_OK:
      len += size / (format * channels);
      break;
    default:
      quit = 1;
    }
  }
  g_rec_mutex_lock(&ao_detail->mutex);
  if (!ao_detail->close) {
    ao_detail->len = len;
    gdk_threads_add_idle(adjustment_conifigure, ao_detail);
  }
  g_rec_mutex_unlock(&ao_detail->mutex);

  mpg123_delete(handle);
  handle = mpg123_new(NULL, &err);
  if (mpg123_open_feed(handle) != MPG123_OK)
    return printf("failed to open feed of mp3 data\n"), NULL;
  if (mpg123_feed(handle, ao_detail->data, ao_detail->data_size) != MPG123_OK)
    return printf("failed to feed mp3 data\n"), NULL;
  PaStream * stream = NULL;
  gdouble now = 0;
  for (int quit = 0; !quit;) {
    switch (mpg123_decode_frame(handle, &frame_offset, &data, &size)) {
    case MPG123_NEW_FORMAT:
      if (mpg123_getformat(handle, &sample_rate, &channels, &enc) != MPG123_OK)
        return printf("failed to get format of mp3 header\n"), NULL;
      format = mpg123_encsize(enc);

      PaSampleFormat sample_format = paInt8;
      if (format == 1)      sample_format = paInt8;
      else if (format == 2) sample_format = paInt16;
      else if (format == 4) sample_format = paFloat32;

      PaStreamParameters params;
      params.device = Pa_GetDefaultOutputDevice();
      params.channelCount = channels;
      params.sampleFormat = sample_format;
      params.suggestedLatency =
          Pa_GetDeviceInfo(params.device)->defaultLowOutputLatency;
      params.hostApiSpecificStreamInfo = NULL;
      if (Pa_OpenStream(&stream, NULL, &params, sample_rate,
                        paFramesPerBufferUnspecified, paNoFlag,
                        NULL, NULL) != paNoError)
        return printf("failed to open the default PortAudio stream\n"), NULL;
      if (Pa_StartStream(stream) != paNoError)
        return printf("failed to start the default PortAudio stream\n"), NULL;

      if (size == 0) continue;
      break;
    case MPG123_OK:
      g_rec_mutex_lock(&ao_detail->mutex);
      int close = ao_detail->close;
      g_rec_mutex_unlock(&ao_detail->mutex);
      if (close) { quit = 1; continue; }
      int frames = size / (format * channels);
      if (Pa_WriteStream(stream, data, frames) != paNoError)
        return printf("failed to write the PortAudio stream\n"), NULL;
      now += frames;
      g_rec_mutex_lock(&ao_detail->mutex);
      if (!ao_detail->close) {
        if (ao_detail->draged_end) {
          gdouble changed = ao_detail->changed;
          gdouble interval = changed + frames - now;
          if (interval >= 0.01 || interval <= -0.01) {
            mpg123_delete(handle);
            handle = mpg123_new(NULL, &err);
            if (mpg123_open_feed(handle) != MPG123_OK)
              return printf("failed to open feed of mp3 data\n"), NULL;
            if (mpg123_feed(handle, ao_detail->data, ao_detail->data_size) != MPG123_OK)
              return printf("failed to feed mp3 data\n"), NULL;
            off_t offset;
            if (mpg123_feedseek(handle, changed, SEEK_CUR, &offset) != MPG123_OK)
              return printf("failed to seek mp3 data\n"), NULL;
            now = changed;
          }
          ao_detail->draged_end = 0;
        }
        ao_detail->now = now;
        ao_detail->idle_id =
            gdk_threads_add_idle(adjustment_set_value, ao_detail);
      }
      g_rec_mutex_unlock(&ao_detail->mutex);
      break;
    case MPG123_NEED_MORE:
      break;
    default:
      break;
    }
    if (size <= 0) {
      g_rec_mutex_lock(&ao_detail->mutex);
      int repeat = ao_detail->repeat;
      g_rec_mutex_unlock(&ao_detail->mutex);
      if (repeat) {
        mpg123_delete(handle);
        handle = mpg123_new(NULL, &err);
        if (mpg123_open_feed(handle) != MPG123_OK)
          return printf("failed to open feed of mp3 data\n"), NULL;
        if (mpg123_feed(handle, ao_detail->data, ao_detail->data_size) != MPG123_OK)
          return printf("failed to feed mp3 data\n"), NULL;
        off_t offset;
        if (mpg123_feedseek(handle, 0, SEEK_CUR, &offset) != MPG123_OK)
          return printf("failed to seek mp3 data\n"), NULL;
        now = 0;
      } else {
        break;
      }
    }
  }
  if (Pa_CloseStream(stream) != paNoError)
    return printf("failed to close the PortAudio stream\n"), NULL;
  mpg123_delete(handle);
  return NULL;
}

void
tree_view_var_on_row_activated(GtkTreeView * tree_view_var,
                               GtkTreePath * path,
                               GtkTreeViewColumn * column, gpointer userdata) {
  FileTreeStore * file_tree_store = userdata;

  GtkWidget * paned_var =
      gtk_widget_get_ancestor(GTK_WIDGET(tree_view_var), GTK_TYPE_PANED);
  GtkWidget * vgrid_window = gtk_paned_get_child2(GTK_PANED(paned_var));
  GtkWidget * viewport = gtk_bin_get_child(GTK_BIN(vgrid_window));
  GtkWidget * origin = gtk_bin_get_child(GTK_BIN(viewport));

  GtkTreeModel * model_var = gtk_tree_view_get_model(tree_view_var);
  GtkTreeIter iter_var;
  if (gtk_tree_model_get_iter(model_var, &iter_var, path) == FALSE) return;
  char * name_bytes;
  char * prim_type_bytes;
  wzvar * var;
  gtk_tree_model_get(model_var, &iter_var,
                     COL_VAR_NAME, &name_bytes,
                     COL_VAR_PRIM_TYPE, &prim_type_bytes,
                     COL_VAR_DATA, &var, -1);

  if (origin != NULL)
    var_grid_destroy(origin);

  GtkWidget * vgrid = var_grid_new();
  gtk_widget_set_margin_top(vgrid, 6);
  gtk_widget_set_margin_left(vgrid, 6);
  gtk_widget_set_margin_right(vgrid, 6);
  gtk_widget_set_margin_bottom(vgrid, 6);
  gtk_widget_show(vgrid);
  gtk_container_add(GTK_CONTAINER(viewport), vgrid);

  GtkWidget * grid_struct = var_grid_insert(vgrid, "Structure");

  int row = 0, col;

  col = var_grid_struct_insert(grid_struct, "name", "STRING", row);
  var_grid_struct_insert_entry(grid_struct, name_bytes, row++, col);

  size_t type_len = strlen(prim_type_bytes);
  char * type_detail = malloc(4 + 1 + 1 + type_len + 1 + 1);
  sprintf(type_detail, "0x%02"PRIx8" (%s)", var->type, prim_type_bytes);
  type_detail[4 + 1 + 1 + type_len + 1] = '\0';
  var_grid_set_type_detail(VAR_GRID(vgrid), type_detail);

  col = var_grid_struct_insert(grid_struct, "type", "UINT8", row);
  var_grid_struct_insert_entry(grid_struct, type_detail, row++, col);

  if (WZ_IS_VAR_INT16(var->type)) {
    char * val = malloc(1 + 5 + 1); // sign + int
    if (val == NULL) { printf("failed to malloc\n"); return; }
    sprintf(val, "%"PRId16, (int16_t) var->val.i);
    var_int_detail * int_detail = malloc(sizeof(* int_detail));
    if (int_detail == NULL) { printf("failed to malloc\n"); return; }
    int_detail->var = var, int_detail->val = val;
    var_detail detail = {.i = int_detail};
    var_grid_set_detail(VAR_GRID(vgrid), detail);
    col = var_grid_struct_insert_value(grid_struct, prim_type_bytes, row);
    var_grid_struct_insert_entry(grid_struct, val, row++, col);
  } else if (WZ_IS_VAR_INT32(var->type)) {
    char * val = malloc(1 + 10 + 1); // sign + int
    if (val == NULL) { printf("failed to malloc\n"); return; }
    sprintf(val, "%"PRId32, (int32_t) var->val.i);
    var_int_detail * int_detail = malloc(sizeof(* int_detail));
    if (int_detail == NULL) { printf("failed to malloc\n"); return; }
    int_detail->var = var, int_detail->val = val;
    var_detail detail = {.i = int_detail};
    var_grid_set_detail(VAR_GRID(vgrid), detail);
    col = var_grid_struct_insert_value(grid_struct, prim_type_bytes, row);
    var_grid_struct_insert_entry(grid_struct, val, row++, col);
  } else if (WZ_IS_VAR_INT64(var->type)) {
    char * val = malloc(1 + 20 + 1); // sign + int
    if (val == NULL) { printf("failed to malloc\n"); return; }
    sprintf(val, "%"PRId64, (int64_t) var->val.i);
    var_int_detail * int_detail = malloc(sizeof(* int_detail));
    if (int_detail == NULL) { printf("failed to malloc\n"); return; }
    int_detail->var = var, int_detail->val = val;
    var_detail detail = {.i = int_detail};
    var_grid_set_detail(VAR_GRID(vgrid), detail);
    col = var_grid_struct_insert_value(grid_struct, prim_type_bytes, row);
    var_grid_struct_insert_entry(grid_struct, val, row++, col);
  } else if (WZ_IS_VAR_FLOAT32(var->type)) {
    char * val = malloc(1 + 39 + 1 + 6 + 1); // sign, int, dot and precision
    if (val == NULL) { printf("failed to malloc\n"); return; }
    float f = (float) var->val.f;
    sprintf(val, "%.6g", f);
    var_flt_detail * flt_detail = malloc(sizeof(* flt_detail));
    if (flt_detail == NULL) { printf("failed to malloc\n"); return; }
    flt_detail->var = var, flt_detail->val = val;
    var_detail detail = {.f = flt_detail};
    var_grid_set_detail(VAR_GRID(vgrid), detail);
    col = var_grid_struct_insert_value(grid_struct, prim_type_bytes, row);
    var_grid_struct_insert_entry(grid_struct, val, row++, col);
  } else if (WZ_IS_VAR_FLOAT64(var->type)) {
    char * val = malloc(1 + 309 + 1 + 6 + 1); // sign, int, dot and precision
    if (val == NULL) { printf("failed to malloc\n"); return; }
    sprintf(val, "%.6g", var->val.f);
    var_flt_detail * flt_detail = malloc(sizeof(* flt_detail));
    if (flt_detail == NULL) { printf("failed to malloc\n"); return; }
    flt_detail->var = var, flt_detail->val = val;
    var_detail detail = {.f = flt_detail};
    var_grid_set_detail(VAR_GRID(vgrid), detail);
    col = var_grid_struct_insert_value(grid_struct, prim_type_bytes, row);
    var_grid_struct_insert_entry(grid_struct, val, row++, col);
  } else if (WZ_IS_VAR_STRING(var->type)) {
    char * bytes = NULL;
    wzchr * val = &var->val.str;
    if (val->enc == WZ_ENC_ASCII) {
      bytes = val->bytes;
    } else if (val->enc == WZ_ENC_UTF16LE) {
      size_t len = 0;
      for (uint32_t i = 0; i < val->len;) {
        uint8_t  utf16le[WZ_UTF16LE_MAX_LEN] = {0};
        size_t   utf16le_len;
        uint32_t code;
        size_t   utf8_len;
        memcpy(utf16le, val->bytes + i,
               val->len - i < sizeof(utf16le) ?
               val->len - i : sizeof(utf16le));
        if (wz_utf16le_len(&utf16le_len, utf16le) ||
            wz_utf16le_to_code(&code, utf16le) ||
            wz_code_to_utf8_len(&utf8_len, code)) return;
        i += (uint32_t) utf16le_len;
        len += utf8_len;
      }
      bytes = malloc(len + 1);
      char * utf8 = bytes;
      for (uint32_t i = 0; i < val->len;) {
        uint8_t   utf16le[WZ_UTF16LE_MAX_LEN] = {0};
        size_t    utf16le_len;
        uint32_t  code;
        size_t    utf8_len;
        memcpy(utf16le, val->bytes + i,
               val->len - i < sizeof(utf16le) ?
               val->len - i : sizeof(utf16le));
        if (wz_utf16le_len(&utf16le_len, utf16le) ||
            wz_utf16le_to_code(&code, utf16le) ||
            wz_code_to_utf8(utf8, code) ||
            wz_code_to_utf8_len(&utf8_len, code)) return;
        i += (uint32_t) utf16le_len;
        utf8 += utf8_len;
      }
      bytes[len] = '\0';
    }
    var_str_detail * str_detail = malloc(sizeof(* str_detail));
    if (str_detail == NULL) { printf("failed to malloc\n"); return; }
    str_detail->var = var, str_detail->bytes = bytes;
    var_detail detail = {.str = str_detail};
    var_grid_set_detail(VAR_GRID(vgrid), detail);
    col = var_grid_struct_insert(grid_struct, "content", prim_type_bytes, row);

    GtkAdjustment * text_window_hadjustment =
        gtk_adjustment_new(0, 0, 0, 0, 0, 0);
    GtkAdjustment * text_window_vadjustment =
        gtk_adjustment_new(0, 0, 0, 0, 0, 0);
    GtkWidget * text_window = gtk_scrolled_window_new(
        text_window_hadjustment, text_window_vadjustment);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(text_window),
                                        GTK_SHADOW_ETCHED_IN);
    gtk_container_set_border_width(GTK_CONTAINER(text_window), 2);
    gtk_widget_set_vexpand(text_window, TRUE);
    gtk_widget_set_hexpand(text_window, TRUE);
    gtk_widget_show(text_window);
    gtk_grid_attach(GTK_GRID(grid_struct), text_window, col, row++, 1, 1);

    GtkTextTagTable * text_tag_table = gtk_text_tag_table_new();
    GtkTextBuffer * text_buffer = gtk_text_buffer_new(text_tag_table);
    gtk_text_buffer_insert_at_cursor(text_buffer, bytes, -1);
    GtkWidget * text_view = gtk_text_view_new_with_buffer(text_buffer);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_CHAR);
    gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(text_view),
                                         GTK_TEXT_WINDOW_TOP, 3);
    gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(text_view),
                                         GTK_TEXT_WINDOW_LEFT, 3);
    gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(text_view),
                                         GTK_TEXT_WINDOW_RIGHT, 3);
    gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(text_view),
                                         GTK_TEXT_WINDOW_BOTTOM, 3);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_widget_set_vexpand(text_view, TRUE);
    gtk_widget_set_hexpand(text_view, TRUE);
    gtk_widget_show(text_view);
    gtk_container_add(GTK_CONTAINER(text_window), text_view);
  } else if (WZ_IS_VAR_OBJECT(var->type)) {
    wzobj * obj = var->val.obj;
    GtkWidget * obj_grid = var_grid_insert(vgrid, "Object Structure");
    gtk_widget_set_margin_right(obj_grid, 3);
    int obj_row = 0, obj_col;
    obj_col = var_grid_struct_insert(obj_grid, "type", "STRING", obj_row);
    var_grid_struct_insert_entry(obj_grid, obj->type.bytes, obj_row++, obj_col);
    if (WZ_IS_OBJ_PROPERTY(&obj->type)) {
      wzlist * list = (wzlist *) obj;

      char * val = malloc(10 + 1); // int
      if (val == NULL) { printf("failed to malloc\n"); return; }
      sprintf(val, "%"PRIu32, (uint32_t) list->len);
      var_list_detail * list_detail = malloc(sizeof(* list_detail));
      if (list_detail == NULL) { printf("failed to malloc\n"); return; }
      list_detail->var = var, list_detail->len = val;
      var_detail detail = {.list = list_detail};
      var_grid_set_detail(VAR_GRID(vgrid), detail);

      char * info = "The number of children";

      obj_col = var_grid_struct_insert(obj_grid, "length", "UINT32", obj_row);
      var_grid_struct_insert_entry(obj_grid, val, obj_row++, obj_col);
      var_grid_struct_insert_label(obj_grid, info, obj_row++, obj_col);
    } else if (WZ_IS_OBJ_CANVAS(&obj->type)) {
      wzimg * img = (wzimg *) obj;

      char * len = malloc(10 + 1); // int
      if (len == NULL) { printf("failed to malloc\n"); return; }
      sprintf(len, "%"PRIu32, (uint32_t) img->len);

      char * w = malloc(10 + 1); // int
      if (w == NULL) { printf("failed to malloc\n"); return; }
      sprintf(w, "%"PRIu32, (uint32_t) img->w);

      char * h = malloc(10 + 1); // int
      if (h == NULL) { printf("failed to malloc\n"); return; }
      sprintf(h, "%"PRIu32, (uint32_t) img->h);

      var_img_detail * img_detail = malloc(sizeof(* img_detail));
      if (img_detail == NULL) { printf("failed to malloc\n"); return; }
      img_detail->var = var;
      img_detail->len = len, img_detail->w = w, img_detail->h = h;
      var_detail detail = {.img = img_detail};
      var_grid_set_detail(VAR_GRID(vgrid), detail);

      char * info = "The number of children";

      obj_col = var_grid_struct_insert(obj_grid, "length", "UINT32", obj_row);
      var_grid_struct_insert_entry(obj_grid, len, obj_row++, obj_col);
      var_grid_struct_insert_label(obj_grid, info, obj_row++, obj_col);

      obj_col = var_grid_struct_insert(obj_grid, "width", "UINT32", obj_row);
      var_grid_struct_insert_entry(obj_grid, w, obj_row++, obj_col);

      obj_col = var_grid_struct_insert(obj_grid, "height", "UINT32", obj_row);
      var_grid_struct_insert_entry(obj_grid, h, obj_row++, obj_col);

      wzcolor * bgra = img->data;
      uint32_t img_pixels = img->w * img->h;
      uint32_t img_size = img_pixels * sizeof(wzcolor);
      uint32_t img_row_len = img->w * sizeof(wzcolor);
      uint8_t * rgba = malloc(img_size);
      for (uint32_t i = 0; i < img_pixels; i++) {
        wzcolor * in = &bgra[i];
        uint8_t * out = &rgba[i * sizeof(wzcolor)];
        out[0] = in->r;
        out[1] = in->g;
        out[2] = in->b;
        out[3] = in->a;
      }
      img_detail->data = rgba;

      GtkWidget * img_grid = gtk_grid_new();
      gtk_widget_set_vexpand(img_grid, FALSE);
      gtk_widget_set_hexpand(img_grid, FALSE);
      gtk_widget_show(img_grid);

      obj_col = var_grid_struct_insert(obj_grid, "data", "UINT8[ ]", obj_row);
      gtk_grid_attach(GTK_GRID(obj_grid), img_grid, obj_col, obj_row++, 1, 1);

      GdkPixbuf * pixbuf = gdk_pixbuf_new_from_data(
          rgba, GDK_COLORSPACE_RGB, TRUE,
          8, img->w, img->h, img_row_len, NULL, NULL);
      GtkWidget * image = gtk_image_new_from_pixbuf(pixbuf);
      GtkStyleContext * style_context = gtk_widget_get_style_context(image);
      gtk_style_context_add_class(style_context, "var-img-data");
      gtk_style_context_add_class(style_context, "clean");
      gtk_widget_set_margin_top(image, 3);
      gtk_widget_set_margin_left(image, 3);
      gtk_widget_set_margin_right(image, 3);
      gtk_widget_show(image);
      g_object_unref(pixbuf);
      gtk_grid_attach_next_to(GTK_GRID(img_grid), image, NULL,
                              GTK_POS_BOTTOM, 1, 1);
    } else if (WZ_IS_OBJ_CONVEX(&obj->type)) {
      wzvex * vex = (wzvex *) obj;

      char * len = malloc(10 + 1); // int
      if (len == NULL) { printf("failed to malloc\n"); return; }
      sprintf(len, "%"PRIu32, vex->len);

      char * info = "The number of vectors";

      obj_col = var_grid_struct_insert(obj_grid, "length", "UINT32", obj_row);
      var_grid_struct_insert_entry(obj_grid, len, obj_row++, obj_col);
      var_grid_struct_insert_label(obj_grid, info, obj_row++, obj_col);

      GtkTreeStore * tree_store = obj_grid_insert_vex(
          obj_grid, "UINT8[ ][2]", vex->vals, vex->len, obj_row++);

      var_2d * vals = malloc(sizeof(* vals) * vex->len);
      if (vals == NULL) { printf("failed to malloc\n"); return; }
      var_vex_detail * vex_detail = malloc(sizeof(* vex_detail));
      if (vex_detail == NULL) { printf("failed to malloc\n"); return; }
      vex_detail->var = var, vex_detail->len = len, vex_detail->vals = vals;
      var_detail detail = {.vex = vex_detail};
      var_grid_set_detail(VAR_GRID(vgrid), detail);

      for (uint32_t i = 0; i < vex->len; i++) {
        wz2d * val = &vex->vals[i];

        char * x = malloc(10 + 1); // int
        if (x == NULL) { printf("failed to malloc\n"); return; }
        sprintf(x, "%"PRId32, val->x);

        char * y = malloc(10 + 1); // int
        if (y == NULL) { printf("failed to malloc\n"); return; }
        sprintf(y, "%"PRId32, val->y);

        vals[i] = (var_2d) {x, y};

        GtkTreeIter iter;
        gtk_tree_store_append(tree_store, &iter, NULL);
        gtk_tree_store_set(tree_store, &iter,
                           COL_VEC_X, x,
                           COL_VEC_Y, y, -1);
      }
    } else if (WZ_IS_OBJ_VECTOR(&obj->type)) {
      wzvec * vec = (wzvec *) obj;

      GtkTreeStore * tree_store = obj_grid_insert_vex(
          obj_grid, "UINT8[2]", &vec->val, 1, obj_row++);

      var_vec_detail * vec_detail = malloc(sizeof(* vec_detail));
      if (vec_detail == NULL) { printf("failed to malloc\n"); return; }
      vec_detail->var = var;
      var_detail detail = {.vec = vec_detail};
      var_grid_set_detail(VAR_GRID(vgrid), detail);

      wz2d * val = &vec->val;

      char * x = malloc(10 + 1); // int
      if (x == NULL) { printf("failed to malloc\n"); return; }
      sprintf(x, "%"PRId32, val->x);

      char * y = malloc(10 + 1); // int
      if (y == NULL) { printf("failed to malloc\n"); return; }
      sprintf(y, "%"PRId32, val->y);

      vec_detail->val = (var_2d) {x, y};

      GtkTreeIter iter;
      gtk_tree_store_append(tree_store, &iter, NULL);
      gtk_tree_store_set(tree_store, &iter,
                         COL_VEC_X, x,
                         COL_VEC_Y, y, -1);
    } else if (WZ_IS_OBJ_SOUND(&obj->type)) {
      wzao * ao = (wzao *) obj;

      char * format = "";
      if (ao->format == WZ_AUDIO_PCM)      format = "WAV data size";
      else if (ao->format == WZ_AUDIO_MP3) format = "MP3 data size";

      uint32_t ao_size = ao->size;
      if (ao->format == WZ_AUDIO_PCM)      ao_size -= WZ_AUDIO_WAV_SIZE;
      else if (ao->format == WZ_AUDIO_MP3) ao_size -= 0;

      char * size = malloc(10 + 2 + strlen(format) + 1 + 1);
      if (size == NULL) { printf("failed to malloc\n"); return; }
      sprintf(size, "%"PRIu32" (%s)", ao_size, format);

      char * ms = malloc(10 + 2 + 10 + 1 + 10 + 9 + 1);
      if (ms == NULL) { printf("failed to malloc\n"); return; }
      sprintf(ms, "%"PRIu32" (%"PRIu32".%03"PRIu32" seconds)",
              ao->ms, ao->ms / 1000, ao->ms % 1000);

      obj_col = var_grid_struct_insert(obj_grid, "size", "UINT32", obj_row);
      var_grid_struct_insert_entry(obj_grid, size, obj_row++, obj_col);

      obj_col = var_grid_struct_insert(obj_grid, "ms", "UINT32", obj_row);
      var_grid_struct_insert_entry(obj_grid, ms, obj_row++, obj_col);

      obj_col = var_grid_struct_insert(obj_grid, "data", "UINT8[ ]", obj_row);

      var_ao_detail * ao_detail = malloc(sizeof(* ao_detail));
      if (ao_detail == NULL) { printf("failed to malloc\n"); return; }
      ao_detail->var = var, ao_detail->size = size, ao_detail->ms = ms;
      var_detail detail = {.ao = ao_detail};
      var_grid_set_detail(VAR_GRID(vgrid), detail);

      if (ao->format == WZ_AUDIO_PCM) {
        wzpcm pcm;
        wz_read_pcm(&pcm, ao->data);
        PaSampleFormat sample_format = paInt8;
        if (pcm.format == 8)      sample_format = paInt8;
        else if (pcm.format = 16) sample_format = paInt16;
        else if (pcm.format = 32) sample_format = paInt32;
        ao_detail->frame_size = pcm.format / 8 * pcm.channels;
        ao_detail->data = pcm.data;
        ao_detail->data_size = ao->size;
        ao_detail->data_read = 0;

        PaStream * stream = NULL;
        PaStreamParameters params;
        params.device = Pa_GetDefaultOutputDevice();
        params.channelCount = pcm.channels;
        params.sampleFormat = sample_format;
        params.suggestedLatency =
            Pa_GetDeviceInfo(params.device)->defaultHighOutputLatency;
        params.hostApiSpecificStreamInfo = NULL;
        if (Pa_OpenStream(&stream, NULL, &params, pcm.sample_rate,
                          paFramesPerBufferUnspecified, paNoFlag,
                          pcm_cb, ao_detail) != paNoError) {
          printf("failed to open the default PortAudio stream\n"); return;
        }
        if (Pa_StartStream(stream) != paNoError) {
          printf("failed to start the PortAudio stream\n"); return;
        }
        ao_detail->stream = stream;
      } else if (ao->format == WZ_AUDIO_MP3) {
        GtkWidget * ao_grid = gtk_grid_new();
        gtk_widget_set_margin_top(ao_grid, 3);
        gtk_widget_show(ao_grid);
        gtk_grid_attach(GTK_GRID(obj_grid), ao_grid, obj_col, obj_row++, 1, 1);

        int ao_row = 0, ao_col = 0;

        GtkAdjustment * adjustment = gtk_adjustment_new(0, 0, 0, 0, 0, 0);
        ao_detail->adjustment = adjustment;

        GtkWidget * scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,
                                          adjustment);
        gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
        gtk_widget_set_hexpand(scale, TRUE);
        gtk_widget_show(scale);
        gtk_grid_attach(GTK_GRID(ao_grid), scale, ao_col++, ao_row, 1, 1);
        g_signal_connect(G_OBJECT(scale), "button-press-event",
                         G_CALLBACK(scale_on_button_press_event), ao_detail);
        g_signal_connect(G_OBJECT(scale), "button-release-event",
                         G_CALLBACK(scale_on_button_release_event), ao_detail);
        g_signal_connect(G_OBJECT(scale), "format-value",
                         G_CALLBACK(scale_on_format_value), ao_detail);

        GtkWidget * button = gtk_toggle_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
        gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
        GtkStyleContext * style_context = gtk_widget_get_style_context(button);
        gtk_style_context_add_class(style_context, "var-ao-data");
        gtk_widget_set_margin_left(button, 3);
        gtk_widget_show(button);
        gtk_grid_attach(GTK_GRID(ao_grid), button, ao_col, ao_row++, 1, 1);
        g_signal_connect(G_OBJECT(button), "toggled",
                         G_CALLBACK(button_on_toggled), ao_detail);

        GtkIconTheme * icon_theme = gtk_icon_theme_get_default();
        GdkPixbuf * pixbuf_repeat = gtk_icon_theme_load_icon(
            icon_theme, "media-playlist-repeat", 16, 0, NULL);

        GtkWidget * image = gtk_image_new_from_pixbuf(pixbuf_repeat);
        gtk_widget_show(image);
        gtk_button_set_image(GTK_BUTTON(button), image);
        g_object_unref(pixbuf_repeat);

        ao_detail->data = ao->data;
        ao_detail->data_size = ao->size;
        ao_detail->close = 0;
        ao_detail->changed = 0;
        ao_detail->draged_begin = 0, ao_detail->draged_end = 0;
        g_rec_mutex_init(&ao_detail->mutex);
        ao_detail->idle_id = 0;
        ao_detail->thread = g_thread_new("mp3", thread_play_mp3, ao_detail);
      }
    } else if (WZ_IS_OBJ_UOL(&obj->type)) {
      wzuol * uol = (wzuol *) obj;
      char * bytes = NULL;
      wzchr * val = &uol->path;
      bytes = val->bytes;
      var_uol_detail * uol_detail = malloc(sizeof(* uol_detail));
      if (uol_detail == NULL) { printf("failed to malloc\n"); return; }
      uol_detail->var = var;
      var_detail detail = {.uol = uol_detail};
      var_grid_set_detail(VAR_GRID(vgrid), detail);
      col = var_grid_struct_insert_value(obj_grid, "STRING", obj_row);
      var_grid_struct_insert_entry(obj_grid, bytes, obj_row++, obj_col);
    }
  }
}

void
button_on_clicked(GtkButton * button, gpointer userdata) {
  GtkWidget * tree_view_var = userdata;
  GtkWidget * tree_view_var_window = gtk_widget_get_parent(tree_view_var);
  GtkWidget * paned_var = gtk_widget_get_parent(tree_view_var_window);
  GtkWidget * vgrid_window = gtk_paned_get_child2(GTK_PANED(paned_var));
  GtkWidget * viewport = gtk_bin_get_child(GTK_BIN(vgrid_window));
  GtkWidget * origin = gtk_bin_get_child(GTK_BIN(viewport));

  if (origin != NULL)
    var_grid_destroy(origin);

  GtkTreeModel * model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view_var));
  GtkTreeStore * tree_store = GTK_TREE_STORE(model);
  GtkTreeIter * stack = malloc(1000000 * sizeof(* stack));
  if (stack == NULL) return;
  GtkTreeIter root_iter, empty;
  memset(&empty, 0, sizeof(empty));
  uint32_t len = 0;
  gboolean root_valid = gtk_tree_model_get_iter_first(model, &root_iter);
  if (root_valid == FALSE) {
    printf("failed to get iter first\n"); return;
  }
  wzvar * first_child;
  gtk_tree_model_get(model, &root_iter, COL_VAR_DATA, &first_child, -1);
  wzvar * root = first_child->parent;
  while (root_valid == TRUE) {
    stack[len++] = root_iter;
    root_valid = gtk_tree_model_iter_next(model, &root_iter);
  }
  while (len) {
    GtkTreeIter * iter = &stack[--len];
    if (memcmp(iter, &empty, sizeof(empty)) == 0) {
      GtkTreeIter * iter_parent = &stack[--len];
      wzvar * parent;
      gtk_tree_model_get(model, iter_parent, COL_VAR_DATA, &parent, -1);
      if (gtk_tree_store_remove(tree_store, iter_parent) == TRUE)
        printf("failed to remove tree view parent node\n");
      wz_free_obj(parent->val.obj);
      continue;
    }
    wzvar * var;
    gtk_tree_model_get(model, iter, COL_VAR_DATA, &var, -1);
    if (!WZ_IS_VAR_OBJECT(var->type)) {
      if (gtk_tree_store_remove(tree_store, iter) == TRUE)
        printf("failed to remove tree view node\n");
      continue;
    }
    wzobj * obj = var->val.obj;
    if (!WZ_IS_OBJ_PROPERTY(&obj->type) &&
        !WZ_IS_OBJ_CANVAS(&obj->type)) {
      if (gtk_tree_store_remove(tree_store, iter) == TRUE)
        printf("failed to remove tree view node\n");
      wz_free_obj(obj);
      continue;
    }
    len++;
    stack[len++] = empty;
    GtkTreeIter child_iter;
    gboolean valid = gtk_tree_model_iter_children(model, &child_iter, iter);
    while (valid) {
      stack[len++] = child_iter;
      valid = gtk_tree_model_iter_next(model, &child_iter);
    }
  }
  wz_free_obj(root->val.obj);
  free(stack);
  gtk_widget_destroy(paned_var);
}

enum {
  COL_NODE_NAME,
  COL_NODE_DATA,
  COL_NODE_LEN
};

int
parse_int(int * out, char * in) {
  int i = 0;
  while (isdigit(* in)) i = i * 10 + * in++;
  if (* in == '\0') return * out = i, 0;
  return 1;
}

int
sort_vars(const void * a, const void * b) {
  uint8_t * x = ((const wzvar *) a)->name.bytes;
  uint8_t * y = ((const wzvar *) b)->name.bytes;
  int m, n;
  if (parse_int(&m, x) || parse_int(&n, y))
    return strcmp(x, y);
  else
    return m - n;
}

void
tree_view_node_on_row_activated(GtkTreeView * tree_view_node,
                                GtkTreePath * path,
                                GtkTreeViewColumn * column, gpointer userdata) {
  GtkNotebook * notebook = userdata;

  GtkTreeModel * model_node = gtk_tree_view_get_model(tree_view_node);
  FileTreeStore * file_tree_store = FILE_TREE_STORE(model_node);
  wzfile * file = file_tree_store_get_wzfile(file_tree_store);
  wzctx * ctx = file_tree_store_get_wzctx(file_tree_store);

  GtkTreeIter iter_node;
  if (gtk_tree_model_get_iter(model_node, &iter_node, path) == FALSE) return;
  char * name_bytes;
  wznode * node;
  gtk_tree_model_get(model_node, &iter_node, COL_NODE_NAME, &name_bytes, -1);
  gtk_tree_model_get(model_node, &iter_node, COL_NODE_DATA, &node, -1);
  if (!WZ_IS_NODE_FILE(node->type)) return;

  GtkWidget * paned_var = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position(GTK_PANED(paned_var), 200);
  gtk_widget_show(paned_var);

  GtkWidget * grid = gtk_grid_new();
  gtk_widget_show(grid);
  gtk_notebook_append_page(notebook, paned_var, grid);
  gint pages = gtk_notebook_get_n_pages(notebook);
  gtk_notebook_set_current_page(notebook, pages - 1);

  GtkWidget * label = gtk_label_new(name_bytes);
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

  GtkWidget * button = gtk_button_new();
  gtk_widget_show(button);
  gtk_grid_attach(GTK_GRID(grid), button, 1, 0, 1, 1);

  GtkIconTheme * icon_theme = gtk_icon_theme_get_default();
  GdkPixbuf * pixbuf_close = gtk_icon_theme_load_icon(
      icon_theme, "window-close", 16, 0, NULL);

  GtkWidget * image = gtk_image_new_from_pixbuf(pixbuf_close);
  gtk_widget_show(image);
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_object_unref(pixbuf_close);

  GtkAdjustment * tree_view_var_window_hadjustment =
      gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  GtkAdjustment * tree_view_var_window_vadjustment =
      gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  GtkWidget * tree_view_var_window = gtk_scrolled_window_new(
      tree_view_var_window_hadjustment, tree_view_var_window_vadjustment);
  gtk_widget_show(tree_view_var_window);
  gtk_paned_pack1(GTK_PANED(paned_var), tree_view_var_window, FALSE, FALSE);

  GtkWidget * tree_view_var = gtk_tree_view_new();
  gtk_widget_set_hexpand(GTK_WIDGET(tree_view_var), TRUE);
  gtk_widget_set_vexpand(GTK_WIDGET(tree_view_var), TRUE);
  gtk_widget_show(tree_view_var);
  gtk_container_add(GTK_CONTAINER(tree_view_var_window), tree_view_var);
  g_signal_connect(G_OBJECT(tree_view_var), "row-activated",
                   G_CALLBACK(tree_view_var_on_row_activated), file_tree_store);

  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(button_on_clicked), tree_view_var);

  GtkCellRenderer * cell_renderer_text = gtk_cell_renderer_text_new();

  GtkTreeViewColumn * column_name = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column_name, "Name");
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_var), column_name);

  gtk_tree_view_column_pack_start(column_name, cell_renderer_text, TRUE);
  gtk_tree_view_column_set_attributes(column_name, cell_renderer_text,
                                      "text", COL_VAR_NAME, NULL);

  GtkTreeViewColumn * column_type = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column_type, "Type");
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_var), column_type);

  gtk_tree_view_column_pack_start(column_type, cell_renderer_text, TRUE);
  gtk_tree_view_column_set_attributes(column_type, cell_renderer_text,
                                      "text", COL_VAR_TYPE, NULL);

  GtkTreeStore * tree_store = gtk_tree_store_new(
      COL_VAR_LEN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_POINTER);
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_var),
                          GTK_TREE_MODEL(tree_store));

  // The right window
  GtkAdjustment * vgrid_window_hadjustment =
      gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  GtkAdjustment * vgrid_window_vadjustment =
      gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  GtkWidget * vgrid_window = gtk_scrolled_window_new(
      vgrid_window_hadjustment, vgrid_window_vadjustment);
  gtk_widget_show(vgrid_window);
  gtk_paned_pack2(GTK_PANED(paned_var), vgrid_window, TRUE, TRUE);

  GtkWidget * viewport = gtk_viewport_new(
      vgrid_window_hadjustment, vgrid_window_vadjustment);
  gtk_widget_show(viewport);
  gtk_container_add(GTK_CONTAINER(vgrid_window), viewport);

  GtkTreeIter * iter_stack = malloc(1000000 * sizeof(* iter_stack));
  if (iter_stack == NULL) return;
  uint32_t iter_len = 1;
  wzvar ** stack = malloc(1000000 * sizeof(* stack));
  if (stack == NULL) return;
  stack[0] = node->data.var;
  for (uint32_t len = 1; len;) {
    wzvar * var = stack[--len];
    GtkTreeIter * iter = &iter_stack[--iter_len];
    if (var == node->data.var) iter = NULL;
    if (WZ_IS_VAR_OBJECT(var->type)) {
      if (wz_read_obj(&var->val.obj, node, file, ctx)) continue;
      wzobj * obj = var->val.obj;
      if (iter != NULL) {
        char * obj_type = "";
        if (WZ_IS_OBJ_PROPERTY(&obj->type))    obj_type = "LIST";
        else if (WZ_IS_OBJ_CANVAS(&obj->type)) obj_type = "IMAGE";
        else if (WZ_IS_OBJ_CONVEX(&obj->type)) obj_type = "CONVEX";
        else if (WZ_IS_OBJ_VECTOR(&obj->type)) obj_type = "VECTOR";
        else if (WZ_IS_OBJ_SOUND(&obj->type))  obj_type = "AUDIO";
        else if (WZ_IS_OBJ_UOL(&obj->type))    obj_type = "UOL";
        gtk_tree_store_set(tree_store, iter,
                           COL_VAR_TYPE, obj_type,
                           COL_VAR_OBJ_TYPE, obj_type, -1);
      }
      if (WZ_IS_OBJ_PROPERTY(&obj->type) ||
          WZ_IS_OBJ_CANVAS(&obj->type)) {
        wzlist * list = (wzlist *) obj;
        GtkTreeIter parent;
        if (iter != NULL) parent = * iter, iter = &parent;
        qsort(list->vars, list->len, sizeof(* list->vars), sort_vars);
        for (uint32_t i = 0; i < list->len; i++) {
          wzvar * child = &list->vars[i];
          char * prim_type = "";
          if (WZ_IS_VAR_NONE(child->type))         prim_type = "NONE";
          else if (WZ_IS_VAR_INT16(child->type))   prim_type = "INT16";
          else if (WZ_IS_VAR_INT32(child->type))   prim_type = "INT32";
          else if (WZ_IS_VAR_INT64(child->type))   prim_type = "INT64";
          else if (WZ_IS_VAR_FLOAT32(child->type)) prim_type = "FLOAT32";
          else if (WZ_IS_VAR_FLOAT64(child->type)) prim_type = "FLOAT64";
          else if (WZ_IS_VAR_STRING(child->type))  prim_type = "STRING";
          else if (WZ_IS_VAR_OBJECT(child->type))  prim_type = "OBJECT";
          GtkTreeIter * child_iter = &iter_stack[iter_len + list->len - i - 1];
          gtk_tree_store_append(tree_store, child_iter, iter);
          gtk_tree_store_set(tree_store, child_iter,
                             COL_VAR_NAME, child->name.bytes,
                             COL_VAR_TYPE, prim_type,
                             COL_VAR_PRIM_TYPE, prim_type,
                             COL_VAR_OBJ_TYPE, "",
                             COL_VAR_DATA, child, -1);
        }
        for (uint32_t i = 0; i < list->len; i++) {
          wzvar * child = &list->vars[list->len - i - 1];
          child->parent = var;
          stack[len++] = child;
        }
        iter_len += list->len;
      }
    }
  }
  free(stack);
  free(iter_stack);
}

void
file_tree_store_cleanup(GtkWidget * tree_view_node) {
  GtkTreeModel * model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view_node));
  FileTreeStore * file_tree_store = FILE_TREE_STORE(model);
  GtkTreeStore * tree_store = GTK_TREE_STORE(file_tree_store);

  g_object_ref(model);
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_node), NULL);

  char * filename = file_tree_store_get_filename(file_tree_store);
  if (filename == NULL) return;
  GtkTreeIter * stack = malloc(10000 * sizeof(* stack));
  if (stack == NULL) return;
  GtkTreeIter root_iter, empty;
  memset(&empty, 0, sizeof(empty));
  uint32_t len = 0;
  gboolean root_valid = gtk_tree_model_get_iter_first(model, &root_iter);
  while (root_valid) {
    stack[len++] = root_iter;
    root_valid = gtk_tree_model_iter_next(model, &root_iter);
  }
  while (len) {
    GtkTreeIter * iter = &stack[--len];
    if (memcmp(iter, &empty, sizeof(empty)) == 0) {
      GtkTreeIter * iter_parent = &stack[--len];
      wznode * parent;
      gtk_tree_model_get(model, iter_parent, COL_NODE_DATA, &parent, -1);
      if (gtk_tree_store_remove(tree_store, iter_parent) == TRUE)
        printf("failed to remove tree view parent node\n");
      wz_free_grp(&parent->data.grp);
      continue;
    }
    wznode * node;
    gtk_tree_model_get(model, iter, COL_NODE_DATA, &node, -1);
    if (!WZ_IS_NODE_DIR(node->type)) {
      if (gtk_tree_store_remove(tree_store, iter) == TRUE)
        printf("failed to remove tree view node\n");
      continue;
    }
    len++;
    stack[len++] = empty;
    GtkTreeIter child_iter;
    gboolean valid = gtk_tree_model_iter_children(model, &child_iter, iter);
    while (valid) {
      stack[len++] = child_iter;
      valid = gtk_tree_model_iter_next(model, &child_iter);
    }
  }
  wzfile * file   = file_tree_store_get_wzfile(file_tree_store);
  wzctx * ctx     = file_tree_store_get_wzctx(file_tree_store);
  wz_free_grp(&file->root.data.grp);
  free(stack);
  wz_free_file(file);
  wz_free_ctx(ctx);
  g_free(filename);
  file_tree_store_set_filename(file_tree_store, NULL);

  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_node), model);
  g_object_unref(model);
}

int
sort_nodes(const void * a, const void * b) {
  const wznode * x = a, * y = b;
  if (WZ_IS_NODE_FILE(x->type) ^ WZ_IS_NODE_FILE(y->type))
    return WZ_IS_NODE_FILE(x->type) - WZ_IS_NODE_FILE(y->type);
  else
    return strcmp(x->name.bytes, y->name.bytes);
}

void
menu_item_open_on_activate(GtkMenuItem * menu_item, gpointer userdata) {
  GtkWidget * tree_view_node = userdata;
  GtkTreeModel * model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view_node));
  FileTreeStore * file_tree_store = FILE_TREE_STORE(model);
  GtkTreeStore * tree_store = GTK_TREE_STORE(file_tree_store);

  GtkFileFilter * filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, "wz files");
  gtk_file_filter_add_pattern(filter, "*.wz");

  GtkWidget * window =
      gtk_widget_get_ancestor(GTK_WIDGET(menu_item), GTK_TYPE_WINDOW);
  GtkWidget * dialog = gtk_file_chooser_dialog_new(
      "Open File", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
      "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
  if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
    gtk_widget_destroy(dialog); return;
  }
  char * filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  gtk_widget_destroy(dialog);

  file_tree_store_cleanup(tree_view_node);
  file_tree_store_set_filename(file_tree_store, filename);

  g_object_ref(model);
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_node), NULL);

  wzctx * ctx = file_tree_store_get_wzctx(file_tree_store);
  wzfile * file = file_tree_store_get_wzfile(file_tree_store);
  if (wz_init_ctx(ctx)) return;
  if (open_file(file, filename, ctx)) { wz_free_ctx(ctx); return; }
  wznode * root = &file->root;
  GtkTreeIter * iter_stack = malloc(10000 * sizeof(* iter_stack));
  if (iter_stack == NULL) return;
  uint32_t iter_len = 1;
  wznode ** stack = malloc(10000 * sizeof(* stack));
  if (stack == NULL) return;
  stack[0] = root;
  for (uint32_t len = 1; len;) {
    wznode * node = stack[--len];
    GtkTreeIter * iter = &iter_stack[--iter_len];
    if (node == root) iter = NULL;
    if (WZ_IS_NODE_DIR(node->type)) {
      if (wz_read_grp(&node->data.grp, node, file, ctx)) {
        printf("failed to read group!\n");
        continue;
      }
      wzgrp * grp = node->data.grp;
      qsort(grp->nodes, grp->len, sizeof(* grp->nodes), sort_nodes);
      for (uint32_t i = 0; i < grp->len; i++) {
        wznode * child = &grp->nodes[i];
        GtkTreeIter * child_iter = &iter_stack[iter_len + grp->len - i - 1];
        gtk_tree_store_append(tree_store, child_iter, iter);
        gtk_tree_store_set(tree_store, child_iter,
                           COL_NODE_NAME, child->name.bytes,
                           COL_NODE_DATA, child, -1);
      }
      for (uint32_t i = 0; i < grp->len; i++) {
        stack[len++] = &grp->nodes[grp->len - i - 1];
        iter_len++;
      }
    }
  }
  free(stack);
  free(iter_stack);

  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_node), model);
  g_object_unref(model);
}

void
menu_item_close_on_activate(GtkMenuItem * menu_item, gpointer userdata) {
  GtkWidget * tree_view_node = userdata;
  file_tree_store_cleanup(tree_view_node);
}

int
main(int argc, char ** argv) {
  gtk_init(&argc, &argv);

  GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "wz viewer");
  gtk_window_set_default_size(GTK_WINDOW(window), 700, 400);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_widget_show(window);

  GtkWidget * grid = gtk_grid_new();
  gtk_widget_show(grid);
  gtk_container_add(GTK_CONTAINER(window), grid);

  GtkWidget * menu_bar = gtk_menu_bar_new();
  gtk_widget_show(menu_bar);
  gtk_grid_attach(GTK_GRID(grid), menu_bar, 0, 0, 1, 1);

  GtkWidget * menu_item_file = gtk_menu_item_new_with_mnemonic("_File");
  gtk_widget_show(menu_item_file);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item_file);

  GtkWidget * menu_file = gtk_menu_new();
  gtk_widget_show(menu_file);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item_file), menu_file);

  GtkWidget * menu_item_open = gtk_menu_item_new_with_mnemonic("Open");
  GtkWidget * menu_item_separator = gtk_separator_menu_item_new();
  GtkWidget * menu_item_close = gtk_menu_item_new_with_mnemonic("Close");
  gtk_widget_show(menu_item_open);
  gtk_widget_show(menu_item_separator);
  gtk_widget_show(menu_item_close);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_file), menu_item_open);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_file), menu_item_separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_file), menu_item_close);

  // The left paned
  GtkWidget * paned_node = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position(GTK_PANED(paned_node), 180);
  gtk_widget_show(paned_node);
  gtk_grid_attach(GTK_GRID(grid), paned_node, 0, 1, 1, 1);

  GtkAdjustment * tree_view_node_window_hadjustment =
      gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  GtkAdjustment * tree_view_node_window_vadjustment =
      gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  GtkWidget * tree_view_node_window = gtk_scrolled_window_new(
      tree_view_node_window_hadjustment, tree_view_node_window_vadjustment);
  gtk_widget_show(tree_view_node_window);
  gtk_paned_pack1(GTK_PANED(paned_node), tree_view_node_window, FALSE, TRUE);

  GtkWidget * tree_view_node = gtk_tree_view_new();
  gtk_widget_set_hexpand(GTK_WIDGET(tree_view_node), TRUE);
  gtk_widget_set_vexpand(GTK_WIDGET(tree_view_node), TRUE);
  gtk_widget_show(tree_view_node);
  gtk_container_add(GTK_CONTAINER(tree_view_node_window), tree_view_node);

  GtkCellRenderer * cell_renderer_text = gtk_cell_renderer_text_new();

  GtkTreeViewColumn * column_name = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column_name, "Name");
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_node), column_name);

  gtk_tree_view_column_pack_start(column_name, cell_renderer_text, TRUE);
  gtk_tree_view_column_set_attributes(column_name, cell_renderer_text,
                                      "text", COL_NODE_NAME, NULL);

  FileTreeStore * file_tree_store = file_tree_store_new();
  GType file_tree_store_types[] = {G_TYPE_STRING, G_TYPE_POINTER};
  gtk_tree_store_set_column_types(GTK_TREE_STORE(file_tree_store),
                                  COL_NODE_LEN, file_tree_store_types);
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_node),
                          GTK_TREE_MODEL(file_tree_store));

  // The right paned
  GtkWidget * notebook = gtk_notebook_new();
  gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
  gtk_widget_show(notebook);
  gtk_paned_pack2(GTK_PANED(paned_node), notebook, TRUE, TRUE);

  g_signal_connect(G_OBJECT(tree_view_node), "row-activated",
                   G_CALLBACK(tree_view_node_on_row_activated), notebook);

  GtkCssProvider * css_provider = gtk_css_provider_new();
  const gchar * css_provider_data =
      "GtkNotebook {\n"
      "  background-color: transparent;\n"
      "}\n"
      "GtkViewport {\n"
      "  background-color: transparent;\n"
      "}\n"
      "GtkImage.var-img-data {\n"
      "  border-width: 2px;\n"
      "  border-color: #888;\n"
      "  border-style: outset;\n"
      "}\n"
      "GtkToggleButton.var-ao-data.clean {\n"
      "  background-image: none;\n"
      "  box-shadow: none;\n"
      "  text-shadow: none;\n"
      "  icon-shadow: none;\n"
      "  border-image: none;\n"
      "}\n";
  gtk_css_provider_load_from_data(css_provider, css_provider_data, -1, NULL);
  GdkDisplay * display = gdk_display_get_default();
  GdkScreen * screen = gdk_display_get_default_screen(display);
  gtk_style_context_add_provider_for_screen(screen,
                                            GTK_STYLE_PROVIDER(css_provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_USER);
  g_object_unref(css_provider);

  wzctx ctx;
  wzfile file;
  file_tree_store_set_filename(file_tree_store, NULL);
  file_tree_store_set_wzfile(file_tree_store, &file);
  file_tree_store_set_wzctx(file_tree_store, &ctx);
  g_signal_connect(G_OBJECT(menu_item_open), "activate",
                   G_CALLBACK(menu_item_open_on_activate), tree_view_node);
  g_signal_connect(G_OBJECT(menu_item_close), "activate",
                   G_CALLBACK(menu_item_close_on_activate), tree_view_node);

  if (Pa_Initialize() != paNoError)
    return printf("failed to initialize PortAudio\n"), 1;
  mpg123_init();

  int exit = 0;
  g_signal_connect(window, "destroy", G_CALLBACK(window_on_destroy), &exit);
  while (!exit) {
    gtk_main_iteration();
  }

  mpg123_exit();
  if (Pa_Terminate() != paNoError)
    return printf("failed to terminate PortAudio\n"), 1;

  return 0;
}
