#include <obs-module.h>
#include <graphics/image-file.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <util/bmem.h>
#include <util/platform.h> // For os_gettime_ns()

// --------------------------------------------------------
// 1. Data Structure
// --------------------------------------------------------
struct png_avatar_data {
	obs_source_t *context;

	gs_image_file_t img_default;
	gs_image_file_t img_talking;
	gs_image_file_t img_shouting;

	double threshold_talking;
	double threshold_shouting;

	// Animation Variables
	int talk_anim_type;  // 0=None, 1=Bounce, 2=Scale, 3=Wobble
	int shout_anim_type; // 0=None, 1=Bounce, 2=Scale, 3=Wobble
	double talking_intensity;
	double shouting_intensity;
	double anim_speed;

	char *current_audio_source_name;
	obs_source_t *active_audio_source;

	float current_db;
	pthread_mutex_t audio_mutex;
};

// --------------------------------------------------------
// 2. Audio Processing Hook
// --------------------------------------------------------
static void audio_capture_callback(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
	struct png_avatar_data *data = param;

	if (muted) {
		pthread_mutex_lock(&data->audio_mutex);
		data->current_db = -100.0f;
		pthread_mutex_unlock(&data->audio_mutex);
		return;
	}

	float peak = 0.0f;
	uint32_t frames = audio_data->frames;
	for (uint32_t i = 0; i < frames; ++i) {
		float val = fabsf(((float *)audio_data->data[0])[i]);
		if (val > peak)
			peak = val;
	}

	float db = (peak > 0.00001f) ? 20.0f * log10f(peak) : -100.0f;

	pthread_mutex_lock(&data->audio_mutex);
	if (db > data->current_db) {
		data->current_db = db;
	} else {
		data->current_db -= 2.0f;
	}
	pthread_mutex_unlock(&data->audio_mutex);
}

// --------------------------------------------------------
// 3. OBS Source Callbacks
// --------------------------------------------------------
static const char *png_avatar_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "PNG Avatar (Audio Reactive)";
}

static void png_avatar_destroy(void *data)
{
	struct png_avatar_data *ctx = data;

	if (ctx->active_audio_source) {
		obs_source_remove_audio_capture_callback(ctx->active_audio_source, audio_capture_callback, ctx);
		obs_source_release(ctx->active_audio_source);
	}
	if (ctx->current_audio_source_name)
		bfree(ctx->current_audio_source_name);

	obs_enter_graphics();
	gs_image_file_free(&ctx->img_default);
	gs_image_file_free(&ctx->img_talking);
	gs_image_file_free(&ctx->img_shouting);
	obs_leave_graphics();

	pthread_mutex_destroy(&ctx->audio_mutex);
	bfree(ctx);
}

static void png_avatar_update(void *data, obs_data_t *settings)
{
	struct png_avatar_data *ctx = data;

	ctx->threshold_talking = obs_data_get_double(settings, "thresh_talking");
	ctx->threshold_shouting = obs_data_get_double(settings, "thresh_shouting");

	ctx->talk_anim_type = (int)obs_data_get_int(settings, "talk_anim_type");
	ctx->shout_anim_type = (int)obs_data_get_int(settings, "shout_anim_type");
	ctx->talking_intensity = obs_data_get_double(settings, "talk_intensity");
	ctx->shouting_intensity = obs_data_get_double(settings, "shout_intensity");
	ctx->anim_speed = obs_data_get_double(settings, "anim_speed");

	obs_enter_graphics();
	gs_image_file_free(&ctx->img_default);
	gs_image_file_free(&ctx->img_talking);
	gs_image_file_free(&ctx->img_shouting);

	gs_image_file_init(&ctx->img_default, obs_data_get_string(settings, "path_default"));
	gs_image_file_init(&ctx->img_talking, obs_data_get_string(settings, "path_talking"));
	gs_image_file_init(&ctx->img_shouting, obs_data_get_string(settings, "path_shouting"));

	gs_image_file_init_texture(&ctx->img_default);
	gs_image_file_init_texture(&ctx->img_talking);
	gs_image_file_init_texture(&ctx->img_shouting);
	obs_leave_graphics();

	const char *audio_source_name = obs_data_get_string(settings, "audio_source");
	if (!ctx->current_audio_source_name || strcmp(ctx->current_audio_source_name, audio_source_name) != 0) {
		if (ctx->active_audio_source) {
			obs_source_remove_audio_capture_callback(ctx->active_audio_source, audio_capture_callback, ctx);
			obs_source_release(ctx->active_audio_source);
		}
		ctx->active_audio_source = obs_get_source_by_name(audio_source_name);
		if (ctx->active_audio_source) {
			obs_source_add_audio_capture_callback(ctx->active_audio_source, audio_capture_callback, ctx);
		}
		if (ctx->current_audio_source_name)
			bfree(ctx->current_audio_source_name);
		ctx->current_audio_source_name = bstrdup(audio_source_name);
	}
}

static void *png_avatar_create(obs_data_t *settings, obs_source_t *source)
{
	struct png_avatar_data *data = bzalloc(sizeof(struct png_avatar_data));
	data->context = source;
	data->current_db = -100.0f;
	pthread_mutex_init(&data->audio_mutex, NULL);

	if (!obs_data_has_user_value(settings, "anim_speed")) {
		obs_data_set_double(settings, "anim_speed", 2.0);
		obs_data_set_double(settings, "talk_intensity", 10.0);
		obs_data_set_double(settings, "shout_intensity", 30.0);
	}

	obs_source_update(source, settings);
	return data;
}

static void png_avatar_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct png_avatar_data *ctx = data;

	float db;
	pthread_mutex_lock(&ctx->audio_mutex);
	db = ctx->current_db;
	pthread_mutex_unlock(&ctx->audio_mutex);

	gs_image_file_t *img_to_draw = &ctx->img_default;
	float current_intensity = 0.0f;
	int current_anim_type = 0;

	if (db >= ctx->threshold_shouting && ctx->img_shouting.texture) {
		img_to_draw = &ctx->img_shouting;
		current_intensity = (float)ctx->shouting_intensity;
		current_anim_type = ctx->shout_anim_type;
	} else if (db >= ctx->threshold_talking && ctx->img_talking.texture) {
		img_to_draw = &ctx->img_talking;
		current_intensity = (float)ctx->talking_intensity;
		current_anim_type = ctx->talk_anim_type;
	} else if (!ctx->img_default.texture) {
		return;
	}

	gs_matrix_push();

	if (current_intensity > 0.0f && current_anim_type > 0) {
		float time_sec = (float)((double)os_gettime_ns() / 1000000000.0);

		float center_x = img_to_draw->cx / 2.0f;
		float center_y = (float)img_to_draw->cy;

		gs_matrix_translate3f(center_x, center_y, 0.0f);

		if (current_anim_type == 1) {
			// 1. BOUNCE
			float bounce_wave = fabsf(sinf(time_sec * (float)ctx->anim_speed * 6.283185f));
			gs_matrix_translate3f(0.0f, -bounce_wave * current_intensity, 0.0f);

		} else if (current_anim_type == 2) {
			// 2. SCALE
			float scale_wave = fabsf(sinf(time_sec * (float)ctx->anim_speed * 6.283185f));
			float scale_factor = 1.0f + (scale_wave * (current_intensity / 100.0f));
			gs_matrix_scale3f(scale_factor, scale_factor, 1.0f);

		} else if (current_anim_type == 3) {
			// 3. WOBBLE
			float wave_x = sinf(time_sec * (float)ctx->anim_speed * 6.283185f);
			float wave_y = cosf(time_sec * (float)ctx->anim_speed * 6.283185f * 2.0f);
			gs_matrix_translate3f(wave_x * (current_intensity * 0.5f), wave_y * (current_intensity * 0.5f),
					      0.0f);
		}

		gs_matrix_translate3f(-center_x, -center_y, 0.0f);
	}

	obs_source_draw(img_to_draw->texture, 0, 0, img_to_draw->cx, img_to_draw->cy, false);
	gs_matrix_pop();
}

static uint32_t png_avatar_get_width(void *data)
{
	return ((struct png_avatar_data *)data)->img_default.cx;
}

static uint32_t png_avatar_get_height(void *data)
{
	return ((struct png_avatar_data *)data)->img_default.cy;
}

// --------------------------------------------------------
// 4. Properties (The OBS UI)
// --------------------------------------------------------
static bool add_audio_source(void *data, obs_source_t *source)
{
	if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) {
		obs_property_list_add_string((obs_property_t *)data, obs_source_get_name(source),
					     obs_source_get_name(source));
	}
	return true;
}

static obs_properties_t *png_avatar_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props, "path_default", "Default Image (Silent)", OBS_PATH_FILE, "Images (*.png *.jpg)",
				NULL);
	obs_properties_add_path(props, "path_talking", "Talking Image", OBS_PATH_FILE, "Images (*.png *.jpg)", NULL);
	obs_properties_add_path(props, "path_shouting", "Shouting Image", OBS_PATH_FILE, "Images (*.png *.jpg)", NULL);

	obs_properties_add_float_slider(props, "thresh_talking", "Talking Threshold (dB)", -60.0, 0.0, 1.0);
	obs_properties_add_float_slider(props, "thresh_shouting", "Shouting Threshold (dB)", -60.0, 0.0, 1.0);

	obs_property_t *audio_list = obs_properties_add_list(props, "audio_source", "Microphone Source",
							     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_enum_sources(add_audio_source, audio_list);

	// --- Talking Animation Settings ---
	obs_property_t *talk_anim_list = obs_properties_add_list(props, "talk_anim_type", "Talking Anim Type",
								 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(talk_anim_list, "None", 0);
	obs_property_list_add_int(talk_anim_list, "Bounce Up", 1);
	obs_property_list_add_int(talk_anim_list, "Scale Up", 2);
	obs_property_list_add_int(talk_anim_list, "Wobble", 3);
	obs_properties_add_float_slider(props, "talk_intensity", "Talking Intensity", 0.0, 100.0, 1.0);

	// --- Shouting Animation Settings ---
	obs_property_t *shout_anim_list = obs_properties_add_list(props, "shout_anim_type", "Shouting Anim Type",
								  OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(shout_anim_list, "None", 0);
	obs_property_list_add_int(shout_anim_list, "Bounce Up", 1);
	obs_property_list_add_int(shout_anim_list, "Scale Up", 2);
	obs_property_list_add_int(shout_anim_list, "Wobble", 3);
	obs_properties_add_float_slider(props, "shout_intensity", "Shouting Intensity", 0.0, 100.0, 1.0);

	obs_properties_add_float_slider(props, "anim_speed", "Global Animation Speed", 0.1, 10.0, 0.1);

	return props;
}

// --------------------------------------------------------
// 5. Plugin Registration
// --------------------------------------------------------
struct obs_source_info png_avatar_source_info = {.id = "obs_png_avatar",
						 .type = OBS_SOURCE_TYPE_INPUT,
						 .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
						 .get_name = png_avatar_get_name,
						 .create = png_avatar_create,
						 .destroy = png_avatar_destroy,
						 .update = png_avatar_update,
						 .video_render = png_avatar_render,
						 .get_width = png_avatar_get_width,
						 .get_height = png_avatar_get_height,
						 .get_properties = png_avatar_properties};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-png-avatar", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "PNG Avatar Audio Reactive Plugin";
}

bool obs_module_load(void)
{
	obs_register_source(&png_avatar_source_info);
	return true;
}