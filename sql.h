#ifndef SQL_H
#define SQL_H

#include <sqlite3.h>
#include <stdarg.h>
#include <stddef.h>

#include "aostr.h"

#define SQL_MAX_QUERY_PARAMS (64)
#define SQL_DB_NAME          "chat-hist.db"

typedef enum SqlType {
    SQL_INT = SQLITE_INTEGER,
    SQL_FLOAT = SQLITE_FLOAT,
    SQL_TEXT = SQLITE3_TEXT,
    SQL_BLOB = SQLITE_BLOB,
    SQL_NULL = SQLITE_NULL,
} SqlType;

typedef struct sqlParam {
    SqlType type;
    union {
        int integer;
        double floating;
        char *str;
        struct {
            void *blob;
            size_t blob_len;
        };
    };
} sqlParam;

typedef struct sqlPreparedStmt {
    sqlite3_stmt *stmt;
    int params_count;
    int *params_type;
} sqlPreparedStmt;

/* We're only storing text so lets not be a hero */
typedef struct sqlColumn {
    long len;
    int type;
    union {
        long integer;
        char *str;
        void *blob;
        double floating;
    };
} sqlColumn;

typedef struct sqlRow {
    sqlite3_stmt *stmt;
    int cols;
    sqlColumn *col;
} sqlRow;

typedef struct sqlCtx {
    sqlite3 *conn;
    char *dbname;
} sqlCtx;


sqlCtx *sqlCtxNew(char *dbname);
/* Unsafe */
char *sqlExecRaw(sqlCtx *ctx, char *sql);

int sqlSelect(sqlCtx *ctx, sqlRow *row, char *stmt, sqlParam *params,
              int count);
int sqlQuery(sqlCtx *ctx, char *sql, sqlParam *params, int param_count);
void sqlRelease(sqlCtx *ctx);
int sqlIter(sqlRow *row);

sqlPreparedStmt *sqlPrepare(sqlCtx *ctx, char *sql);
int sqlExecPrepared(sqlPreparedStmt *pstmt, sqlRow *row, sqlParam *params);
int sqlIterPrepared(sqlRow *row);
void sqlFinalizePrepared(sqlPreparedStmt *pstmt);

#endif
