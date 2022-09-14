#ifndef DB_H
#define DB_H

#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#define AFK_SECOND 60 // away from keyboard

typedef struct {
    int user_id;
    bool is_online; // 0: offline, 1: online
    long last_update_ts; // time_t now; time(&now); printf("%ld\n", now);
} db;

bool get_online_status(int db_fd, int user_id) {

    int i, ret;
    db buf;

    for (i = 0;; i++) {
        ret = pread(db_fd, &buf, sizeof(db), sizeof(db) * i);
        if (ret <= 0) {
            break;
        }

        if (buf.user_id == user_id) {
            if (buf.is_online) {
                return true; // explicit true (records found and online)
            } else {
                return false; // explicit false (records found and offline)
            }
        }
    }

    return false; // implicit false (no records)
}

void put_online_status(int db_fd, int user_id, bool is_online) {

    int i, ret;
    db buf;

    for (i = 0;; i++) {
        ret = pread(db_fd, &buf, sizeof(db), sizeof(db) * i);
        if (ret <= 0) {
            break;
        }

        if (buf.user_id == user_id) {
            // records found, update

            buf.is_online = is_online;
            time_t now;
            time(&now);
            buf.last_update_ts = (long)now;

            pwrite(db_fd, &buf, sizeof(db), sizeof(db) * i);

            return;
        }
    }

    // records not found
    buf.user_id = user_id;
    buf.is_online = is_online;
    time_t now;
    time(&now);
    buf.last_update_ts = (long)now;

    pwrite(db_fd, &buf, sizeof(db), sizeof(db) * i);

    return;
}

#endif /* DB_H */