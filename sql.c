/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "aostr.h"
#include "list.h"
#include "panic.h"
#include "sql.h"

sqlCtx *sqlCtxNew(char *dbname) {
    sqlite3 *db;
    int ok = sqlite3_open(dbname, &db);
    if (ok != SQLITE_OK) {
        const char *s = sqlite3_errmsg(db);
        printf("NOT OK: %s\n", s);
        return NULL;
    }
    sqlCtx *ctx = (sqlCtx *)malloc(sizeof(sqlCtx));
    ctx->dbname = dbname;
    ctx->conn = db;
    return ctx;
}

void sqlRelease(sqlCtx *ctx) {
    sqlite3_close(ctx->conn);
    free(ctx);
}

sqlPreparedStmt *sqlPrepare(sqlCtx *ctx, char *sql) {
    sqlPreparedStmt *pstmt = (sqlPreparedStmt *)malloc(sizeof(sqlPreparedStmt));
    sqlite3_prepare_v2(ctx->conn, sql, -1, &(pstmt->stmt), NULL);
    pstmt->params_count = sqlite3_bind_parameter_count(pstmt->stmt);
    pstmt->params_type = (int *)malloc(pstmt->params_count * sizeof(int));
    return pstmt;
}

static void sqlReset(sqlPreparedStmt *pstmt) {
    sqlite3_clear_bindings(pstmt->stmt);
    sqlite3_reset(pstmt->stmt);
}

void sqlFinalizePrepared(sqlPreparedStmt *pstmt) {
    sqlite3_finalize(pstmt->stmt);
    free(pstmt->params_type);
    free(pstmt);
}

char *sqlExecRaw(sqlCtx *ctx, char *sql) {
    char *errmsg;
    int rc = sqlite3_exec(ctx->conn, sql, 0, 0, &errmsg);
    if (!rc) {
        return errmsg;
    }
    return NULL;
}

int sqlExecPrepared(sqlPreparedStmt *pstmt, sqlRow *row, sqlParam *params) {
    int rc = 0;
    sqlReset(pstmt);

    if (row) {
        row->stmt = NULL;
    }

    for (int i = 0; i < pstmt->params_count; ++i) {
        sqlParam param = params[i];
        switch (param.type) {
        case SQL_INT:
            sqlite3_bind_int64(pstmt->stmt, i + 1, param.integer);
            break;
        case SQL_FLOAT:
            sqlite3_bind_double(pstmt->stmt, i + 1, param.floating);
            break;
        case SQL_TEXT:
            sqlite3_bind_text(pstmt->stmt, i + 1, param.str, -1, NULL);
            break;
        case SQL_BLOB:
            sqlite3_bind_blob(pstmt->stmt, i + 1, param.blob, param.blob_len,
                              NULL);
            break;
        case SQL_NULL:
            sqlite3_bind_text(pstmt->stmt, i + 1, NULL, -1, NULL);
            break;
        }
    }

    /* Execute. */
    rc = sqlite3_step(pstmt->stmt);
    if (rc == SQLITE_ROW && row) {
        row->stmt = pstmt->stmt;
        row->cols = 0;
        row->col = NULL;
    }

    return rc;
}

static int sqlExecQuery(sqlCtx *ctx, sqlRow *row, char *sql, sqlParam *params,
                        int param_count) {
    int rc = SQLITE_ERROR;
    sqlite3_stmt *stmt = NULL;

    if (row) {
        row->stmt = NULL;
    }

    rc = sqlite3_prepare_v2(ctx->conn, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        warning("QUERY not ok: %s\n", sql);
        goto err;
    }

    for (int i = 0; i < param_count; ++i) {
        switch (params[i].type) {
        case SQL_INT:
            /* The leftmost SQL parameter has an index of 1 ref:
             * https://www.sqlite.org/c3ref/bind_blob.html */
            rc = sqlite3_bind_int64(stmt, i + 1, params[i].integer);
            break;
        case SQL_FLOAT:
            rc = sqlite3_bind_double(stmt, i + 1, params[i].floating);
            break;
        case SQL_TEXT:
            rc = sqlite3_bind_text(stmt, i + 1, params[i].str, -1, NULL);
            break;
        case SQL_BLOB:
            rc = sqlite3_bind_blob(stmt, i + 1, params[i].blob,
                                   params[i].blob_len, NULL);
            break;
        default:
            goto err;
        }
        if (rc != SQLITE_OK) {
            goto err;
        }
    }

    /* Execute. */
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW && row) {
        row->stmt = stmt;
        row->cols = 0;
        row->col = NULL;
        stmt = NULL;
    }

err:
    if (stmt) {
        sqlite3_finalize(stmt);
    }
    return rc;
}

void sqlRowRelease(sqlRow *row) {
    if (row->stmt == NULL) {
        return;
    }
    free(row->col);
    sqlite3_finalize(row->stmt);
    row->col = NULL;
    row->stmt = NULL;
}

int sqlSelect(sqlCtx *ctx, sqlRow *row, char *sql, sqlParam *params,
              int param_count) {
    return sqlExecQuery(ctx, row, sql, params, param_count);
}

int sqlQuery(sqlCtx *ctx, char *sql, sqlParam *params, int param_count) {
    return sqlExecQuery(ctx, NULL, sql, params, param_count) == SQLITE_DONE;
}

static int sqlIterGeneric(sqlRow *row, int free_row) {
    if (row->stmt == NULL) {
        return 0;
    }

    if (row->col != NULL) {
        if (sqlite3_step(row->stmt) != SQLITE_ROW) {
            if (free_row) {
                sqlRowRelease(row);
            }
            return 0;
        }
    }

    free(row->col);
    row->cols = sqlite3_data_count(row->stmt);
    row->col = (sqlColumn *)malloc(row->cols * sizeof(sqlColumn));

    for (int i = 0; i < row->cols; i++) {
        row->col[i].type = sqlite3_column_type(row->stmt, i);
        switch (row->col[i].type) {
        case SQLITE_INTEGER:
            row->col[i].integer = sqlite3_column_int64(row->stmt, i);
            break;
        case SQLITE_FLOAT:
            row->col[i].floating = sqlite3_column_double(row->stmt, i);
            break;
        case SQLITE3_TEXT:
            row->col[i].str = (char *)sqlite3_column_text(row->stmt, i);
            row->col[i].len = sqlite3_column_bytes(row->stmt, i);
            break;
        case SQLITE_BLOB:
            row->col[i].blob = (void *)sqlite3_column_blob(row->stmt, i);
            row->col[i].len = sqlite3_column_bytes(row->stmt, i);
            break;
        default:
            // You may want to handle this case differently
            row->col[i].str = NULL;
            row->col[i].integer = 0;
            row->col[i].floating = 0;
            break;
        }
    }
    return 1;
}

int sqlIter(sqlRow *row) {
    return sqlIterGeneric(row, 1);
}

int sqlIterPrepared(sqlRow *row) {
    return sqlIterGeneric(row, 0);
}
