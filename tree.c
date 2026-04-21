#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pes.h"
#include "tree.h"
#include "index.h"



/*
Convert Tree struct → byte format
*/
int tree_serialize(const Tree *tree,
                   void **data_out,
                   size_t *len_out)
{
    size_t total = 0;

    // calculate required size
    for (int i = 0; i < tree->count; i++)
    {
        total += 32 + strlen(tree->entries[i].name) + 1;
    }

    char *buf = malloc(total);

    if (!buf) return -1;

    size_t pos = 0;

    for (int i = 0; i < tree->count; i++)
    {
        TreeEntry *e = &tree->entries[i];

        // mode + space + name
        pos += sprintf(buf + pos,
                       "%o %s",
                       e->mode,
                       e->name);

        // store hash bytes
        memcpy(buf + pos,
               e->hash.hash,
               HASH_SIZE);

        pos += HASH_SIZE;
    }

    *data_out = buf;
    *len_out = pos;

    return 0;
}



/*
Convert stored bytes → Tree struct
*/
int tree_parse(const void *data,
               size_t len,
               Tree *tree)
{
    size_t pos = 0;

    tree->count = 0;

    while (pos < len)
    {
        TreeEntry *e =
            &tree->entries[tree->count++];

        sscanf((char*)data + pos,
               "%o %255s",
               &e->mode,
               e->name);

        pos += strlen(e->name) + 1;

        memcpy(e->hash.hash,
               (char*)data + pos,
               HASH_SIZE);

        pos += HASH_SIZE;
    }

    return 0;
}



/*
Recursive helper for building directory tree
*/
static int write_tree_level(IndexEntry *entries,
                            int count,
                            const char *prefix,
                            ObjectID *id_out)
{
    Tree tree;

    tree.count = 0;

    int i = 0;

    while (i < count)
    {
        const char *path = entries[i].path;

        const char *rel = path + strlen(prefix);

        const char *slash = strchr(rel, '/');


        // FILE
        if (!slash)
        {
            TreeEntry *e = &tree.entries[tree.count++];

            e->mode = entries[i].mode;

            strncpy(e->name,
                    rel,
                    sizeof(e->name) - 1);

            e->name[sizeof(e->name) - 1] = '\0';

            e->hash = entries[i].hash;

            i++;
        }


        // DIRECTORY
        else
        {
            size_t dir_len = slash - rel;

            char dirname[256];

            strncpy(dirname,
                    rel,
                    dir_len);

            dirname[dir_len] = '\0';


            char new_prefix[512];

            snprintf(new_prefix,
                     sizeof(new_prefix),
                     "%s%s/",
                     prefix,
                     dirname);


            int j = i;

            while (j < count)
            {
                const char *next_rel =
                    entries[j].path + strlen(prefix);

                if (strncmp(next_rel,
                            dirname,
                            dir_len) != 0)
                    break;

                if (next_rel[dir_len] != '/')
                    break;

                j++;
            }


            ObjectID subtree_id;

            if (write_tree_level(entries + i,
                                 j - i,
                                 new_prefix,
                                 &subtree_id) != 0)
                return -1;


            TreeEntry *e = &tree.entries[tree.count++];

            e->mode = 040000;

            strncpy(e->name,
                    dirname,
                    sizeof(e->name) - 1);

            e->name[sizeof(e->name) - 1] = '\0';

            e->hash = subtree_id;

            i = j;
        }
    }


    void *tree_data;

    size_t tree_len;

    if (tree_serialize(&tree,
                       &tree_data,
                       &tree_len) != 0)
        return -1;


    int result =
        object_write(OBJ_TREE,
                     tree_data,
                     tree_len,
                     id_out);

    free(tree_data);

    return result;
}



/*
Main function required by assignment
*/
int tree_from_index(ObjectID *id_out)
{
    Index index;

    index.count = 0;

    if (index_load(&index) != 0)
        return -1;

    if (index.count == 0)
        return -1;


    return write_tree_level(index.entries,
                            index.count,
                            "",
                            id_out);
}
