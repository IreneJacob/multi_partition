#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<readline/readline.h>
#include<readline/history.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include "functions.h"
#include "hashmap.h"

struct rocksdb_txdb_params txrocks;

// REVIEW: where do we use this macro?
#define MAX_NUM_ELEMENTS 10

int done;
hashmap * hmap;


int com_open PARAMS((char *));
int com_close PARAMS((char *));
int com_write PARAMS((char *));
int com_tx_create PARAMS((char *));
int com_tx_commit PARAMS((char *));
int com_tx_destroy PARAMS((char *));
int com_read PARAMS((char *));
int com_tx_list PARAMS((char *));
int com_list PARAMS((char *));
int com_delete PARAMS((char *));
int com_tx_set_savepoint PARAMS((char *));
int com_tx_rollback PARAMS((char *));
int com_help PARAMS((char *));
int com_quit PARAMS((char *));
int com_db_destroy PARAMS((char *));

typedef struct {
    char *name;			/* User printable name of the function. */
    rl_icpfunc_t *func;		/* Function to call to do the job. */
    char *doc;			/* Documentation for this function.  */
} COMMAND;

COMMAND commands[] = {
    { "open", com_open, "Open a transaction database" },
    { "tx_create", com_tx_create, "Create a transaction" },
    { "write", com_write, "Write a (key,value) pair" },
    { "read", com_read, "Read the value associated with a key" },
    { "delete", com_delete, "Delete a key" },
    { "commit", com_tx_commit, "Commit the transaction" },
    { "tx_destroy", com_tx_destroy, "Destroy the transaction" },
    { "save", com_tx_set_savepoint, "Set a save point for the transaction" },
    { "rollback", com_tx_rollback, "Rollback the transaction to previously saved point" },
    { "tx_list", com_tx_list, "List all keys and values in the transaction" },
    { "list", com_list, "List all keys and values in the database"},
    { "help", com_help, "Display this text" },
    { "?", com_help, "Synonym for `help'" },
    { "close", com_close, "Close the opened database" },
    { "destroy", com_db_destroy, "delete the database" },
    { "quit", com_quit, "Quit using rocks" },
    { "exit", com_quit, "Synonym for `quit'" },
    { (char *)NULL, (rl_icpfunc_t *)NULL, (char *)NULL }
};

/* Forward declarations */
void initialize_readline();
char * stripwhite (char *string);

/* **************************************************************** */
/*                                                                  */
/*                  Interface to Readline Completion                */
/*                                                                  */
/* **************************************************************** */
char *command_generator PARAMS((const char *, int));
char **rocks_completion PARAMS((const char *, int, int));
int invalid_argument(char *caller, char *arg);
COMMAND* find_command (char *name);
int execute_line(char *s);
static int lcore_hello(__attribute__((unused)) void *arg);

int main(int argc, char* argv[]) {
    int ret;
    unsigned lcore_id;

    // initialize the Hashmap
    hmap = create_map();

    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
      rte_panic("Cannot init EAL\n");
    }

    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
      rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
    }

  	/* call it on master lcore too */
  	lcore_hello(NULL);

  	rte_eal_mp_wait_lcore();
    initialize_readline();
    // init_rocksdb_txdb();

    char* line;
    char* s;

    while(!done)
    {
        line = readline("rocks> ");
        if (!line) {
            break;
        }

        s = stripwhite (line);

        if (*s)
        {
            add_history(s);
            execute_line(s);
        }

        free(line);
    }

    txdb_deconstruct();
    // destroy_map(hmap);
    return 0;
}

static int lcore_hello(__attribute__((unused)) void *arg){
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	printf("hello from core %u\n", lcore_id);
  char db_id[5];

  snprintf(db_id, sizeof(db_id), "%u", lcore_id);
  init_rocksdb_txdb();
  return open_txdb(db_id);
	// return 0;
}

int execute_line(char *line) {
    COMMAND *command;
    char *word = NULL;
    register int i;
    /* Isolate the command word. */
    i = 0;
    while (line[i] && whitespace (line[i]))
        i++;
    word = line + i;

    while (line[i] && !whitespace (line[i]))
        i++;

    if (line[i])
        line[i++] = '\0';

    command = find_command(word);

    if (!command)
    {
        fprintf(stderr, "%s No such command.\n", word);
        return -1;
    }

    /* Get argument to command, if any. */
    while (whitespace (line[i]))
      i++;

    word = line + i;

    /* Call the function. */
    return ((*(command->func)) (word));
}

/* Strip whitespace from the start and end of STRING.  Return a pointer
   into STRING. */
char * stripwhite (char *string)
{
    register char *s, *t;

    for (s = string; whitespace (*s); s++)
    ;

    if (*s == 0)
        return (s);

    t = s + strlen (s) - 1;
    while (t > s && whitespace (*t))
        t--;
    *++t = '\0';

    return s;
}

/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
COMMAND* find_command (char *name)
{
    int i;
    for (i = 0; commands[i].name; i++)
        if (strcmp (name, commands[i].name) == 0)
            return (&commands[i]);
    return ((COMMAND *)NULL);
}

/* **************************************************************** */
/*                                                                  */
/*                  Interface to Readline Completion                */
/*                                                                  */
/* **************************************************************** */


/* Attempt to complete on the contents of TEXT.  START and END bound the
   region of rl_line_buffer that contains the word to complete.  TEXT is
   the word to complete.  We can use the entire contents of rl_line_buffer
   in case we want to do some simple parsing.  Return the array of matches,
   or NULL if there aren't any. */
char **
rocks_completion (const char *text, int start, int end)
{
    char **matches;

    matches = (char **)NULL;

    /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
    if (start == 0)
        matches = rl_completion_matches (text, command_generator);

    return (matches);
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *
command_generator (const char *text, int state)
{
    static int list_index, len;
    char *name;

    /* If this is a new word to complete, initialize now.  This includes
       saving the length of TEXT for efficiency, and initializing the index
       variable to 0. */
    if (!state)
    {
        list_index = 0;
        len = strlen (text);
    }

    /* Return the next name which partially matches from the command list. */
    while ((name = commands[list_index].name))
    {
        list_index++;
        if (strncmp (name, text, len) == 0)
            return (strdup(name));
    }

    /* If no names matched, then return NULL. */
    return ((char *)NULL);
}


/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */
void initialize_readline ()
{
    /* Allow conditional parsing of the ~/.inputrc file. */
    rl_readline_name = "rocks";
    /* Tell the completer that we want a crack first. */
    rl_attempted_completion_function = rocks_completion;
}

int com_open(char *arg)
{
    if (invalid_argument("open", arg))
        return -1;
    return open_txdb(arg);
}

int com_db_destroy(char *arg)
{
    if (invalid_argument("destroy", arg))
        return -1;
    return destroy_txdb(arg);
}

int com_read(char *arg)
{
    if (invalid_argument("read", arg))
        return -1;
    return handle_tx_read(arg, strlen(arg));
}


int com_write(char *arg)
{
    if (invalid_argument("write", arg))
        return -1;
    const char s[2] = " ";
    char *key;
    char *value;
    /* get the first token */
    key = strtok(arg, s);

    if (key == NULL) {
        fprintf(stderr, "Key is empty\n");
        return -1;
    }
    value = strtok(NULL, s);
    if (value == NULL) {
        fprintf(stderr, "Value is empty\n");
        return -1;
    }
    // int ret = map_insert_node(hmap, key);
    int partition = handle_map_insert(hmap, key);
    if (partition < 0) {
      fprintf(stderr, "Key could not be stored in hashmap\n");
      return -1;
    }
    // TODO: use the partition id returned to assign to the correct thread
    return handle_tx_write(key, strlen(key), value, strlen(value) + 1);

    return 0;
}


int com_delete(char *arg)
{
    if (invalid_argument("delete", arg))
        return -1;
    int ret = map_remove_node(hmap, arg);
    return handle_tx_delete(arg, strlen(arg));
}

/* Print out help for ARG, or for all of the commands if ARG is
   not present. */
int com_help (char *arg)
{
    register int i;
    int printed = 0;

    for (i = 0; commands[i].name; i++)
    {
        if (!*arg || (strcmp (arg, commands[i].name) == 0))
        {
            printf ("%10s\t%s.\n", commands[i].name, commands[i].doc);
            printed++;
        }
    }

    if (!printed)
    {
        printf ("No commands match `%s'.  Possibilties are:\n", arg);

    for (i = 0; commands[i].name; i++)
    {
        /* Print in six columns. */
        if (printed == 6)
        {
            printed = 0;
            printf ("\n");
        }

        printf ("%s\t", commands[i].name);
            printed++;
        }

        if (printed)
            printf ("\n");
    }
    return 0;
}

int com_list(char *arg)
{
    return handle_txdb_list();
}

int com_tx_list(char *arg)
{
    return handle_tx_list();
}

int com_close(char *arg)
{
    int ret = map_rollback_to_checkpoint(hmap);
    return close_txdb();
}

int com_tx_create(char *arg)
{
    return begin_tx();
}

int com_tx_commit(char *arg)
{
    int ret = map_set_checkpoint(hmap);
    return commit_tx();
}

int com_tx_destroy(char *arg)
{
    int ret = map_rollback_to_checkpoint(hmap);
    return delete_tx();
}

int com_tx_set_savepoint(char *arg)
{
    int ret = map_set_checkpoint(hmap);
    return handle_save();
}

int com_tx_rollback(char *arg)
{
    int ret = map_rollback_to_checkpoint(hmap);
    return handle_rollback();
}


/* The user wishes to quit using this program.  Just set DONE non-zero. */
int com_quit (char *arg)
{
    // TODO: should I destroy the map here or simply rollback?
    // int ret = map_rollback_to_checkpoint(hmap);
    destroy_map(hmap);
    done = 1;
    return 0;
}

/* Return zero if ARG is a valid argument for CALLER, else print
   an error message and return -1. */
int
invalid_argument(char *caller, char *arg)
{
    if (!arg || !*arg)
    {
        fprintf (stderr, "%s: Argument required.\n", caller);
        return -1;
    }
  return 0;
}
