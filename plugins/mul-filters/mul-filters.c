#include <obs-module.h>
#include "mul-filters-config.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mul-filters", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Multimedia filters";
}

extern struct obs_source_info mirror_filter;
extern struct obs_source_info color_inverse_filter;
extern struct obs_source_info radial_wave_filter;
extern struct obs_source_info pixelize_filter;
extern struct obs_source_info gauss_filter;
extern struct obs_source_info edge_detection_filter;

bool obs_module_load(void)
{
	obs_register_source(&mirror_filter);
	obs_register_source(&color_inverse_filter);
	obs_register_source(&radial_wave_filter);
	obs_register_source(&pixelize_filter);
	obs_register_source(&gauss_filter);
	obs_register_source(&edge_detection_filter);
	return true;
}
