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

#define TEXT_STRENGTH                     obs_module_text("Strength")

struct gauss_filter_data {
	obs_source_t                   *context;
	gs_effect_t                    *effect;
	gs_eparam_t                    *strength_param;
	gs_eparam_t                    *width_param;
	gs_eparam_t                    *height_param;
	gs_eparam_t                    *kernel_param;
    struct matrix4                  kernel_matrix;
    float                           strength;
};

/* Calculate Gauss coefficient at (x,y) with sigma 
 * Algorithm taken from https://homepages.inf.ed.ac.uk/rbf/HIPR2/gsmooth.htm */
float gaussCoefficientAt(float x, float y, float sigma)
{
    return (1.0/2.0*M_PI*sigma*sigma)*exp(-(x*x+y*y)/(2*sigma*sigma));
}

void constructKernel(struct matrix4* out_matrix, float sigma_parameter)
{
	*out_matrix = (struct matrix4)
	{
		gaussCoefficientAt(-2,-2,sigma_parameter), gaussCoefficientAt(-1,-2,sigma_parameter), gaussCoefficientAt(1,-2,sigma_parameter), gaussCoefficientAt(2,-2,sigma_parameter),
		gaussCoefficientAt(-2,-1,sigma_parameter), gaussCoefficientAt(-1,-1,sigma_parameter), gaussCoefficientAt(1,-1,sigma_parameter), gaussCoefficientAt(2,-1,sigma_parameter),
		gaussCoefficientAt(-2,1,sigma_parameter), gaussCoefficientAt(-1,1,sigma_parameter), gaussCoefficientAt(1,1,sigma_parameter), gaussCoefficientAt(2,1,sigma_parameter),
		gaussCoefficientAt(-2,2,sigma_parameter), gaussCoefficientAt(-1,2,sigma_parameter), gaussCoefficientAt(1,2,sigma_parameter), gaussCoefficientAt(2,2,sigma_parameter)
	};
}


/*
 * As the functions' namesake, this provides the internal name of your Filter,
 * which is then translated/referenced in the "data/locale" files.
 */
static const char *gauss_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Gauss filter");
}

/*
 * This function is called (see bottom of this file for more details)
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU.
 */
static void gauss_filter_update(void *data, obs_data_t *settings)
{
	struct gauss_filter_data *filter = data;

	/* Build our Gamma numbers. */
	double gamma = obs_data_get_double(settings, SETTING_STRENGTH);
	filter->strength = gamma;

    constructKernel(&filter->kernel_matrix, filter->strength);
	gs_effect_set_matrix4(filter->kernel_param, &filter->kernel_matrix);

}

/*
 * Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters.
 */
static void gauss_filter_destroy(void *data)
{
	struct gauss_filter_data *filter = data;

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
static void *gauss_filter_create(obs_data_t *settings,
	obs_source_t *context)
{
	/*
	* Because of limitations of pre-c99 compilers, you can't create an
	* array that doesn't have a known size at compile time. The below
	* function calculates the size needed and allocates memory to
	* handle the source.
	*/
	struct gauss_filter_data *filter =
		bzalloc(sizeof(struct gauss_filter_data));

	/*
	 * By default the effect file is stored in the ./data directory that
	 * your filter resides in.
	 */
	char *effect_path = obs_module_file("gauss_filter.effect");

	filter->context = context;

	/* Here we enter the GPU drawing/shader portion of our code. */
	obs_enter_graphics();

    char* errorString;
	/* Load the shader on the GPU. */
	filter->effect = gs_effect_create_from_file(effect_path, &errorString);

	/* If the filter is active pass the parameters to the filter. */
	if (filter->effect) {
		filter->strength_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_STRENGTH);
		filter->width_param = gs_effect_get_param_by_name(
				filter->effect, "width_param");
		filter->height_param = gs_effect_get_param_by_name(
				filter->effect, "height_param");
                
		filter->kernel_param = gs_effect_get_param_by_name(
				filter->effect, "kernel_matrix");
	} else {
        blog(LOG_ERROR, "Failed to load gauss effect shader: %s\n",errorString);
    }

	obs_leave_graphics();

	bfree(effect_path);

	/*
	 * If the filter has been removed/deactivated, destroy the filter
	 * and exit out so we don't crash OBS by telling it to update
	 * values that don't exist anymore.
	 */
	if (!filter->effect) {
		gauss_filter_destroy(filter);
		return NULL;
	}

	/*
	 * It's important to call the update function here. If we don't
	 * we could end up with the user controlled sliders and values
	 * updating, but the visuals not updating to match.
	 */
	gauss_filter_update(filter, settings);
	return filter;
}

/* This is where the actual rendering of the filter takes place. */
static void gauss_filter_render(void *data, gs_effect_t *effect)
{
	struct gauss_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
			OBS_ALLOW_DIRECT_RENDERING))
		return;

	/* Now pass the interface variables to the .effect file. */
	gs_effect_set_float(filter->strength_param, filter->strength);
    constructKernel(&filter->kernel_matrix, filter->strength);
	//gs_effect_set_matrix4(filter->kernel_param, &filter->kernel_matrix);

	/*float width = (float)obs_source_get_width(
			obs_filter_get_target(filter->context));
	float height = (float)obs_source_get_height(
			obs_filter_get_target(filter->context));
	gs_effect_set_float(filter->width_param, width);
	gs_effect_set_float(filter->height_param, height);
    */

	gs_effect_set_float(filter->width_param, 1000.0);
	gs_effect_set_float(filter->height_param, 1000.0);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/*
 * This function sets the interface. the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *gauss_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_float_slider(props, SETTING_STRENGTH,
			TEXT_STRENGTH, 0.1, 0.5, 0.001);

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
static void gauss_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SETTING_STRENGTH, 0.1);
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
struct obs_source_info gauss_filter = {
	.id = "gauss_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = gauss_filter_name,
	.create = gauss_filter_create,
	.destroy = gauss_filter_destroy,
	.video_render = gauss_filter_render,
	.update = gauss_filter_update,
	.get_properties = gauss_filter_properties,
	.get_defaults = gauss_filter_defaults
};
