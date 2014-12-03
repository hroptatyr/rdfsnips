
#include "ttl-split.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = 1;
		goto out;
	}

out:
	yuck_free(argi);
	return rc;
}

/* ttl-split.c ends here */
