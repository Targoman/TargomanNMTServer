#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-value"
#include <marian/common/config.h>
#pragma GCC diagnostic pop

static marian::Ptr<marian::Options> CommandLineOptions;
#define EXTRA_DEBUG(body) if(CommandLineOptions->get<bool>("extra_debug", false)) { body }
