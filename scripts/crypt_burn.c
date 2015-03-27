/*
 * Copyright (C) 2010 Martin Willi
 * Copyright (C) 2010 revosec AG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <stdio.h>
#include <library.h>

static int burn_crypter(const proposal_token_t *token, u_int limit)
{
	chunk_t iv,  data;
	crypter_t *crypter;
	int i = 0;
	bool ok;

	crypter = lib->crypto->create_crypter(lib->crypto, token->algorithm,
										  token->keysize / 8);
	if (!crypter)
	{
		fprintf(stderr, "%N-%zu not supported\n",
				encryption_algorithm_names, token->algorithm, token->keysize);
		return FALSE;
	}

	iv = chunk_alloc(crypter->get_iv_size(crypter));
	memset(iv.ptr, 0xFF, iv.len);
	data = chunk_alloc(round_up(1024, crypter->get_block_size(crypter)));
	memset(data.ptr, 0xDD, data.len);

	ok = TRUE;
	while (ok)
	{
		if (!crypter->encrypt(crypter, data, iv, NULL))
		{
			fprintf(stderr, "encryption failed!\n");
			ok = FALSE;
			break;
		}
		if (!crypter->decrypt(crypter, data, iv, NULL))
		{
			fprintf(stderr, "decryption failed!\n");
			ok = FALSE;
			break;
		}
		if (limit && ++i == limit)
		{
			break;
		}
	}
	crypter->destroy(crypter);

	free(iv.ptr);
	free(data.ptr);

	return ok;
}

static bool burn_aead(const proposal_token_t *token, u_int limit)
{
	chunk_t iv, data, dataicv, assoc;
	aead_t *aead;
	int i = 0;
	bool ok;

	aead = lib->crypto->create_aead(lib->crypto, token->algorithm,
									token->keysize / 8, 0);
	if (!aead)
	{
		fprintf(stderr, "%N-%zu not supported\n",
				encryption_algorithm_names, token->algorithm, token->keysize);
		return FALSE;
	}

	iv = chunk_alloc(aead->get_iv_size(aead));
	memset(iv.ptr, 0xFF, iv.len);
	dataicv = chunk_alloc(round_up(1024, aead->get_block_size(aead)) +
						  aead->get_icv_size(aead));
	data = chunk_create(dataicv.ptr, dataicv.len - aead->get_icv_size(aead));
	memset(data.ptr, 0xDD, data.len);
	assoc = chunk_alloc(13);
	memset(assoc.ptr, 0xCC, assoc.len);

	ok = TRUE;
	while (ok)
	{
		if (!aead->encrypt(aead, data, assoc, iv, NULL))
		{
			fprintf(stderr, "aead encryption failed!\n");
			ok = FALSE;
			break;
		}
		if (!aead->decrypt(aead, dataicv, assoc, iv, NULL))
		{
			fprintf(stderr, "aead integrity check failed!\n");
			ok = FALSE;
			break;
		}
		if (limit && ++i == limit)
		{
			break;
		}
	}
	aead->destroy(aead);

	free(iv.ptr);
	free(data.ptr);

	return ok;
}

int main(int argc, char *argv[])
{
	const proposal_token_t *token;
	int limit = 0;
	bool ok;

	library_init(NULL, "crypt_burn");
	lib->plugins->load(lib->plugins, getenv("PLUGINS") ?: PLUGINS);
	atexit(library_deinit);

	fprintf(stderr, "loaded: %s\n", lib->plugins->loaded_plugins(lib->plugins));

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <algorithm>!\n", argv[0]);
		return 1;
	}
	if (argc > 2)
	{
		limit = atoi(argv[2]);
	}

	token = lib->proposal->get_token(lib->proposal, argv[1]);
	if (!token)
	{
		fprintf(stderr, "algorithm '%s' unknown!\n", argv[1]);
		return 1;
	}

	switch (token->type)
	{
		case ENCRYPTION_ALGORITHM:
			if (encryption_algorithm_is_aead(token->algorithm))
			{
				ok = burn_aead(token, limit);
			}
			else
			{
				ok = burn_crypter(token, limit);
			}
			break;
		default:
			fprintf(stderr, "'%s' is not a crypter/aead algorithm!\n", argv[1]);
			ok = FALSE;
			break;
	}
	return !ok;
}
