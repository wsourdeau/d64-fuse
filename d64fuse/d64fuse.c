#define FUSE_USE_VERSION 35
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fuse.h>

#include "utils.h"
#include "context.h"
#include "operations.h"

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
  int result;
  struct fuse_opt option_spec[] = {
    OPTION ("-I %s", image_filename, 0),
    OPTION ("--image=%s", image_filename, 0),
    OPTION ("-h", show_help, 1),
    OPTION ("--help", show_help, 1),
    FUSE_OPT_END
  };

  result = fuse_opt_parse (args, options_ptr, option_spec, NULL);
  if (result == 0)
    {
      if (options_ptr->show_help)
        fuse_opt_add_arg (args, "--help");
      else if (is_null(options_ptr->image_filename))
        {
          fprintf (stderr, "missing '--image' parameter\n");
          result = -1;
        }
    }

  return result;
}

int main (int argc, char * argv[])
{
  int result = 0;
  d64fuse_options options;
  struct fuse_args args = FUSE_ARGS_INIT (argc, argv);

  if (parse_args (&args, &options) == 0)
    {
      d64fuse_context context;
      memset (&context, 0, sizeof (context));
      context.nbr_files = -1;

      if (options.show_help)
        show_help (argv[0]);
      else
        context.image_filename = options.image_filename;

      result = fuse_main (args.argc, args.argv, &operations, &context);
      fuse_opt_free_args (&args);
    }
  else
    result = -1;

  return result;
}
