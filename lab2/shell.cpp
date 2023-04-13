// IO
#include <iostream>
// std::string
#include <string>
// C-type string
#include <cstring>
// std::vector
#include <vector>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <sys/wait.h>

std::vector<std::string> split(std::string s, const std::string &delimiter);

void process_pwd() {
    auto buf = get_current_dir_name();
    if (buf) {
        std::cout << buf << std::endl;
        free(buf);
    } else {
        int err = errno;
        switch (err) {
        case EACCES:
            std::cout << "Permission denied.\n";
            break;
        case ENOENT:
            std::cout << "The current working directory has been unlinked.\n";
            break;
        case ENOMEM:
            std::cout << "No enough memory!\n";
            break;
        default:
            std::cout << "Unknown error occured.\n";
            break;
        }
    }
}

void process_cd(std::vector<std::string> &args, char *home_dir) {
    if (args.size() > 2) {
        std::cout << "Too many arguments!\n";
        return;
    }

    const char *buf;
    std::string path;

    if (args.size() == 1) {
        // 根据bash的行为，参数为空时进入用户主目录
        buf = home_dir;
    } else {
        path = std::string{args[1]};
        if (args[1][0] == '~') {
            // 将~替换为主目录
            path.replace(0, 1, std::string{home_dir});
        }
        buf = path.data();
    }

    auto ret = chdir(buf);
    if (ret == -1) {
        int err = errno;
        if (err == EACCES) {
            std::cout << "Permission denied.\n";
        } else if (err == EFAULT) {
            std::cout << "Path points outside your accessible address "
                         "space.\n";
        } else if (err == EIO) {
            std::cout << "An I/O error occurred.\n";
        } else if (err == ELOOP) {
            std::cout << "Too many symbolic links were encountered in "
                         "resolving path.\n";
        } else if (err == ENAMETOOLONG) {
            std::cout << "Path is too long!\n";
        } else if (err == ENOENT) {
            std::cout << "Directory does not exist!\n";
        } else if (err == ENOMEM) {
            std::cout << "Insufficient kernel memory was available.\n";
        } else if (err == ENOTDIR) {
            std::cout << "A component of path is not a directory.\n";
        } else {
            std::cout << "Unknown error occurred.\n";
        }
    }
}

int main() {
    // 不同步 iostream 和 cstdio 的 buffer
    std::ios::sync_with_stdio(false);

    // 用来存储读入的一行命令
    std::string cmd;

    // 获取用户主目录
    char *home_dir = getenv("HOME");

    while (true) {
        // 打印提示符
        std::cout << "# ";

        // 读入一行。std::getline 结果不包含换行符。
        std::getline(std::cin, cmd);

        // 按空格分割命令为单词
        std::vector<std::string> args = split(cmd, " ");

        // 没有可处理的命令
        if (args.empty()) {
            continue;
        }

        // 退出
        if (args[0] == "exit") {
            if (args.size() <= 1) {
                return 0;
            }

            // std::string 转 int
            std::stringstream code_stream(args[1]);
            int code = 0;
            code_stream >> code;

            // 转换失败
            if (!code_stream.eof() || code_stream.fail()) {
                std::cout << "Invalid exit code\n";
                continue;
            }

            return code;
        }

        if (args[0] == "pwd") {
            process_pwd();
            continue;
        }

        if (args[0] == "cd") {
            process_cd(args, home_dir);
            continue;
        }

        // 处理外部命令
        pid_t pid = fork();

        // std::vector<std::string> 转 char **
        char *arg_ptrs[args.size() + 1];
        for (auto i = 0; i < (int)args.size(); i++) {
            arg_ptrs[i] = (char *)malloc(args[i].length() + 1);
            auto buf = args[i].c_str();
            strcpy(arg_ptrs[i], buf);
        }
        // exec p 系列的 argv 需要以 nullptr 结尾
        arg_ptrs[args.size()] = nullptr;

        if (pid == 0) {
            // 这里只有子进程才会进入
            // execvp 会完全更换子进程接下来的代码，所以正常情况下 execvp
            // 之后这里的代码就没意义了 如果 execvp 之后的代码被运行了，那就是
            // execvp 出问题了
            execvp(args[0].c_str(), arg_ptrs);

            // 所以这里直接报错
            exit(255);
        }

        // 这里只有父进程（原进程）才会进入
        int ret = wait(nullptr);
        if (ret < 0) {
            std::cout << "wait failed";
        }
    }
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    res.push_back(s);
    return res;
}
