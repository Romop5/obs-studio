/*****************************************************************************
Copyright (C) 2019 by Roman Dobias <rom.dobias@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/
#include <obs-module.h>
#include <graphics/matrix4.h>
#include <graphics/quat.h>


#define SETTING_STRENGTH                  "strength"
#define SETTING_DUTYCYCLE                 "dutyCycle"
#define SETTING_TICKCOUNT                 "tickCount"
#define SETTING_APARAM                    "a"
#define SETTING_BPARAM                    "b"

#define SETTING_XMATRIX                   "x_points"
#define SETTING_YMATRIX                   "y_points"

#define TEXT_STRENGTH                     obs_module_text("Strength")
#define TEXT_DUTYCYCLE                    obs_module_text("Duty cycle")
#define TEXT_A                            obs_module_text("Parameter A")
#define TEXT_B                            obs_module_text("Parameter B")

struct noise_generator_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;

	gs_eparam_t                    *strength_param;
	gs_eparam_t                    *dutyCycle_param;
	gs_eparam_t                    *tickcount_param;

	gs_eparam_t                    *a_param;
	gs_eparam_t                    *b_param;

	gs_eparam_t                    *xMatrix_param;
	gs_eparam_t                    *yMatrix_param;

    float                           aValue;
    float                           bValue;
    float                           strength;
    float                           dutyCycle;
    int                             tickcount;
};

/*
 * As the functions' namesake, this provides the internal name of your Filter,
 * which is then translated/referenced in the "data/locale" files.
 */
static const char *noise_generator_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Noise generator");
}

/*
 * This function is called (see bottom of this file for more details)
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU.
 */
static void noise_generator_filter_update(void *data, obs_data_t *settings)
{
	struct noise_generator_filter_data *filter = data;

	/* Build our Gamma numbers. */
	double gamma = obs_data_get_double(settings, SETTING_STRENGTH);
	filter->strength = gamma;
	double dutyCycle = obs_data_get_double(settings, SETTING_DUTYCYCLE);
	filter->dutyCycle = dutyCycle;

	double aParam = obs_data_get_double(settings, SETTING_APARAM);
	filter->aValue = aParam;

	double bParam = obs_data_get_double(settings, SETTING_BPARAM);
	filter->bValue = bParam;
}

/*
 * Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters.
 */
static void noise_generator_filter_destroy(void *data)
{
	struct noise_generator_filter_data *filter = data;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		obs_leave_graphics();
	}

	bfree(data);
}

/*
 * When you apply a filter OBS creates it, and adds it to the source. OBS also
 * starts rendering it immediately. This function doesn't just 'create' the
 * filter, it also calls the render function (farther below) that contains the
 * actual rendering code.
 */
static void *noise_generator_filter_create(obs_data_t *settings,
	obs_source_t *context)
{
	/*
	* Because of limitations of pre-c99 compilers, you can't create an
	* array that doesn't have a known size at compile time. The below
	* function calculates the size needed and allocates memory to
	* handle the source.
	*/
	struct noise_generator_filter_data *filter =
		bzalloc(sizeof(struct noise_generator_filter_data));

    filter->tickcount = 0;

	/*
	 * By default the effect file is stored in the ./data directory that
	 * your filter resides in.
	 */
	char *effect_path = obs_module_file("noise_generator_filter.effect");

	filter->context = context;

	/* Here we enter the GPU drawing/shader portion of our code. */
	obs_enter_graphics();

	/* Load the shader on the GPU. */
	filter->effect = gs_effect_create_from_file(effect_path, NULL);

	/* If the filter is active pass the parameters to the filter. */
	if (filter->effect) {
		filter->strength_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_STRENGTH);
		filter->dutyCycle_param = gs_effect_get_param_by_name(
				filter->effect, "dutyCycle");
		filter->tickcount_param= gs_effect_get_param_by_name(
				filter->effect, SETTING_TICKCOUNT);
		filter->a_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_APARAM);
		filter->b_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_BPARAM);

		filter->xMatrix_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_XMATRIX);
		filter->yMatrix_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_YMATRIX);
	}

	obs_leave_graphics();

	bfree(effect_path);

	/*
	 * If the filter has been removed/deactivated, destroy the filter
	 * and exit out so we don't crash OBS by telling it to update
	 * values that don't exist anymore.
	 */
	if (!filter->effect) {
		noise_generator_filter_destroy(filter);
		return NULL;
	}

	/*
	 * It's important to call the update function here. If we don't
	 * we could end up with the user controlled sliders and values
	 * updating, but the visuals not updating to match.
	 */
	noise_generator_filter_update(filter, settings);
	return filter;
}

float uniformRand()
{
    float result = ((float) rand()) / RAND_MAX;
    //blog(LOG_INFO, "%f \n", result);
    return result;
}

/* This is where the actual rendering of the filter takes place. */
static void noise_generator_filter_render(void *data, gs_effect_t *effect)
{
	struct noise_generator_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
			OBS_ALLOW_DIRECT_RENDERING))
		return;

	/* Now pass the interface variables to the .effect file. */
	gs_effect_set_float(filter->strength_param, filter->strength);
	gs_effect_set_float(filter->dutyCycle_param, filter->dutyCycle);
    gs_effect_set_int(filter->tickcount_param, filter->tickcount++);

	gs_effect_set_float(filter->a_param, filter->aValue);
	gs_effect_set_float(filter->b_param, filter->bValue);


    static const float y_mat[16] = {0.182586f, -0.100644f,  0.439216f, 0.0f,
                                  0.614231f, -0.338572f, -0.398942f, 0.0f,
                                  0.062007f,  0.439216f, -0.040274f, 0.0f,
                                  0.062745f,  0.501961f,  0.501961f, 1.0f};

    static int counter = 0;
    if((counter % ((int) filter->bValue)) == 0)
    {
        struct matrix4 xMat = (struct matrix4)
        {
           uniformRand(), uniformRand(), uniformRand(), uniformRand(),
           uniformRand(), uniformRand(), uniformRand(), uniformRand(),
           uniformRand(), uniformRand(), uniformRand(), uniformRand(),
           uniformRand(), uniformRand(), uniformRand(), uniformRand()
        };
        struct matrix4 yMat = (struct matrix4)
        {
           uniformRand(), uniformRand(), uniformRand(), uniformRand(),
           uniformRand(), uniformRand(), uniformRand(), uniformRand(),
           uniformRand(), uniformRand(), uniformRand(), uniformRand(),
           uniformRand(), uniformRand(), uniformRand(), uniformRand()
        };

        gs_effect_set_matrix4(filter->xMatrix_param, &xMat);
        gs_effect_set_matrix4(filter->yMatrix_param, &yMat);
    }
    counter++;

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/*
 * This function sets the interface. the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *noise_generator_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_float_slider(props, SETTING_STRENGTH,
			TEXT_STRENGTH, 0.0, 0.5, 0.01);

	obs_properties_add_float_slider(props, SETTING_DUTYCYCLE,
			TEXT_DUTYCYCLE, 0.0, 1.0, 0.01);

	obs_properties_add_float_slider(props, SETTING_APARAM,
			TEXT_A, 0.0, 10.0, 0.1);

	obs_properties_add_float_slider(props, SETTING_BPARAM,
			TEXT_B, 1.0, 100.0, 1.00);

	UNUSED_PARAMETER(data);
	return props;
}

/*
 * As the functions' namesake, this provides the default settings for any
 * options you wish to provide a default for. Try to select defaults that
 * make sense to the end user, or that don't effect the data.
 * *NOTE* this function is completely optional, as is providing a default
 * for any particular setting.
 */
static void noise_generator_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SETTING_STRENGTH, 0.1);
	obs_data_set_default_double(settings, SETTING_DUTYCYCLE, 0.5);
}

/*
 * So how does OBS keep track of all these plug-ins/filters? How does OBS know
 * which function to call when it needs to update a setting? Or a source? Or
 * what type of source this is?
 *
 * OBS does it through the obs_source_info_struct. Notice how variables are
 * assigned the name of a function? Notice how the function name has the
 * variable name in it? While not mandatory, it helps a ton for you (and those
 * reading your code) to follow this convention.
 */
struct obs_source_info noise_generator_filter = {
	.id = "noise_generator_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = noise_generator_filter_name,
	.create = noise_generator_filter_create,
	.destroy = noise_generator_filter_destroy,
	.video_render = noise_generator_filter_render,
	.update = noise_generator_filter_update,
	.get_properties = noise_generator_filter_properties,
	.get_defaults = noise_generator_filter_defaults
};
