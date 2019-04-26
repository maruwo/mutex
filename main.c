#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pthread.h>

static pthread_mutex_t* mutex;
static int shmid;
static bool created = true;

void mutex_demo(void)
{
    int i;
    struct timeval now;
    struct tm *local;
    char str[32];

    for (i = 0; i < 10; i++)
    {
        // ロック
        if (pthread_mutex_lock(mutex) != 0) {
            return;
        }

        gettimeofday(&now, NULL);
        local = localtime( &now.tv_sec );
        printf("%02d:%02d:%02d.%06ld : count=%d, pid=%d\n",
                local->tm_hour, local->tm_min, local->tm_sec, now.tv_usec,
                i+1, getpid());

        sleep(1);

        // アンロック
        if (pthread_mutex_unlock(mutex) != 0) {
            return;
        }

        usleep( 1 * 1000 );
    }
}

bool mutex_init(void)
{
    key_t key;
    pthread_mutexattr_t mat;
    char path[256];

    // 自分のフルパスを取得
    readlink( "/proc/self/exe", path, sizeof(path) );

    // 自分のパスを元にkeyを作成
    key = ftok(path, 1);
    if( key == (key_t)-1 ) {
        return false;
    }

    // 共有メモリ作成
    shmid = shmget(key, sizeof(pthread_mutex_t), IPC_CREAT | IPC_EXCL | 0600);
    if (shmid < 0) {
        if (errno != EEXIST) {
            // エラー
            return false;
        }
        // すでに作成済みの場合はID取得
        shmid = shmget(key, sizeof(pthread_mutex_t), 0600);
        created = false;
    }

    // 共有メモリを取得
    mutex = (pthread_mutex_t*)shmat(shmid, NULL, 0);
    if (mutex == (pthread_mutex_t*)-1) {
        return false;
    }

    // 新規作成のため初期化する
    if (created) {
        // mutex 属性オブジェクト attr を初期化し、すべての属性をデフォルトの値に設定
        if( pthread_mutexattr_init(&mat) != 0 ) {
            return false;
        }

        // mutex 属性オブジェクト attr の属性 pshared を 設定
        if (pthread_mutexattr_setpshared(&mat, PTHREAD_PROCESS_SHARED) != 0) {
            return false;
        }

        // mat で指定された mutex 属性オブジェクトに従って初期化する
        if (pthread_mutex_init(mutex, &mat) != 0) {
            return false;
        }
    }

    return true;
}


void main(void)
{
    int pid, status;
    bool result = false;

    // 初期化
    if( !mutex_init() ) {
        goto ERROR;
    }

    // mutex デモ
    pid = fork();
    mutex_demo();
    if( pid == 0 ) {
        exit(0);
    }
    waitpid(pid, &status, 0);
    result = true;

ERROR:
    if( !created ) {
        // mutex 破棄
        pthread_mutex_destroy(mutex);
        // 共有メモリ破棄
        shmctl(shmid, IPC_RMID, NULL);
    }

    if( !result ) {
        exit(1);
    }
}
