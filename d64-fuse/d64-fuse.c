#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fuse.h>

#include "d64fuse_context.h"
#include "operations.h"
#include "utils.h"

typedef struct d64fuse_options {
  char *image_filename;
  int show_help;
} d64fuse_options;

#define OPTION(t, p, v) { t, offsetof (struct d64fuse_options, p), v }

static void show_help (const char *progname)
{
  fprintf (stderr, "usage: %s --image=[image{.d64,.d71,.d81}] <mountpoint>\n", progname);
}

int parse_args(struct fuse_args *args, d64fuse_options *options_ptr)
{
  struct fuse_opt option_spec[] = {
    OPTION ("-I %s", image_filename, 0),
    OPTION ("--image=%s", image_filename, 0),
    OPTION ("-h", show_help, 1),
    OPTION ("--help", show_help, 1),
    FUSE_OPT_END
  };

  int result = fuse_opt_parse (args, options_ptr, option_spec, NULL);
  if (result == 0)
    {
      if (options_ptr->show_help)
        fuse_opt_add_arg (args, "--help");
      else if (is_null(options_ptr->image_filename))
        {
          fprintf (stderr, "missing '--image' parameter\n");
          result = -1;
        }

      if (access (options_ptr->image_filename, R_OK) != 0)
        {
          perror ("Invalid image parameter");
          return -1;
        }
    }

  return result;
}

d64fuse_context make_context (const d64fuse_options * options)
{
  d64fuse_context context;

  memset (&context, 0, sizeof (context));
  context.nbr_files = -1;
  context.image_filename = canonicalize_file_name (options->image_filename);

  return context;
}

int run_d64fuse (const d64fuse_options * options, const struct fuse_args *args)
{
  d64fuse_context context = make_context (options);
  int result = fuse_main (args->argc, args->argv, &operations, &context);
  free (context.image_filename);

  return result;
}

int main (int argc, char * argv[])
{
  struct fuse_args args = FUSE_ARGS_INIT (argc, argv);
  d64fuse_options options;

  if (parse_args (&args, &options) != 0)
    return -1;

  if (options.show_help)
    {
      show_help (argv[0]);
      return 0;
    }

  int result = run_d64fuse (&options, &args);
  fuse_opt_free_args (&args);

  return result;
}
