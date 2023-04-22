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
// cmd queue
#include <queue>
// file
#include <fcntl.h>

std::vector<std::string> split(std::string s, const std::string &delimiter);

std::queue<std::vector<std::string>> vstr_split(std::vector<std::string> &vstr,
                                                const std::string &delimiter);

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

void process_one(std::vector<std::string> &args, int left_pipe[2],
                 int right_pipe[2]);

void process_out(std::queue<std::vector<std::string>> &cmds, int last_pipe[2]) {
    auto args = cmds.front();
    cmds.pop();
    if (!cmds.empty()) {
        int pipe_out[2];
        if (pipe(pipe_out) == -1) {
            std::cerr << "Pipe create failed!" << std::endl;
            return;
        }
        pid_t pid = fork();
        if (pid == 0) {
            process_one(args, last_pipe, pipe_out);
        } else {
            if (last_pipe) {
                close(last_pipe[0]);
                close(last_pipe[1]);
            }
            process_out(cmds, pipe_out);
        }
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            process_one(args, last_pipe, 0);
        } else {
            if (last_pipe) {
                close(last_pipe[0]);
                close(last_pipe[1]);
            }
        }
    }
}

void process_one(std::vector<std::string> &args, int left_pipe[2],
                 int right_pipe[2]) {
#ifdef TEST
    std::cerr << getpid() << " running args: ";
    for (auto s : args) {
        std::cerr << s << " ";
    }
    std::cerr << std::endl;
#endif
    if (args.size() >= 4) {
        int n = args.size();
        if ((args[n - 4] == "<" || args[n - 4] == "<<" ||
             args[n - 4] == "<<<") &&
            (args[n - 2] == ">" || args[n - 2] == ">>")) {
            std::swap(args[n - 4], args[n - 2]);
            std::swap(args[n - 3], args[n - 1]);
        }
    }
    if (left_pipe) {
        dup2(left_pipe[0], STDIN_FILENO);
        close(left_pipe[0]);
        close(left_pipe[1]);
    } else {
        if (args.size() >= 2) {
            int n = args.size();
            if (args[n - 2] == "<") {
                std::string filename = args.back();
                args.pop_back();
                args.pop_back();
                int file_in = open(filename.data(), O_RDONLY);
                if (file_in != -1) {
                    dup2(file_in, STDIN_FILENO);
                    close(file_in);
                } else {
                    std::cout << "Failed to open file " << filename << "!"
                              << std::endl;
                    return;
                }
            } else if (args[n - 2] == "<<") {
                std::string split_str = args.back();
                args.pop_back();
                args.pop_back();
                int tmp_file = open("./", O_TMPFILE | O_RDWR);
                if (tmp_file != -1) {
                    std::string line;
                    while (std::getline(std::cin, line), line != split_str) {
                        line += "\n";
                        write(tmp_file, line.data(), line.length());
                    }
                    lseek(tmp_file, SEEK_SET, 0);
                    dup2(tmp_file, STDIN_FILENO);
                    close(tmp_file);
                } else {
                    std::cout << "Failed to create temp file!" << std::endl;
                    return;
                }
            } else if (args[n - 2] == "<<<") {
                std::string temp_str = args.back();
                args.pop_back();
                args.pop_back();
                int tmp_file = open("./", O_TMPFILE | O_RDWR);
                if (tmp_file != -1) {
                    temp_str += "\n";
                    write(tmp_file, temp_str.data(), temp_str.length());
                    lseek(tmp_file, SEEK_SET, 0);
                    dup2(tmp_file, STDIN_FILENO);
                    close(tmp_file);
                } else {
                    std::cout << "Failed to create temp file!" << std::endl;
                    return;
                }
            }
        }
    }
    if (right_pipe) {
        dup2(right_pipe[1], STDOUT_FILENO);
        close(right_pipe[0]);
        close(right_pipe[1]);
    } else {
        if (args.size() >= 2) {
            int n = args.size();
            if (args[n - 2] == ">" || args[n - 2] == ">>") {
                std::string filename = args.back();
                args.pop_back();
                std::string op = args.back();
                args.pop_back();
                int file_in;
                if (op == ">")
                    file_in = open(filename.data(),
                                   O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU);
                else
                    file_in = open(filename.data(),
                                   O_WRONLY | O_APPEND | O_CREAT, S_IRWXU);
                if (file_in != -1) {
                    dup2(file_in, STDOUT_FILENO);
                    close(file_in);
                } else {
                    std::cout << "Failed to open/create file " << filename
                              << "!" << std::endl;
                    return;
                }
            }
        }
    }
    // std::vector<std::string> 转 char **
    char *arg_ptrs[args.size() + 1];
    for (auto i = 0; i < (int)args.size(); i++) {
        arg_ptrs[i] = (char *)malloc(args[i].length() + 1);
        auto buf = args[i].c_str();
        strcpy(arg_ptrs[i], buf);
    }

    // exec p 系列的 argv 需要以 nullptr 结尾
    arg_ptrs[args.size()] = nullptr;

    execvp(args[0].c_str(), arg_ptrs);
    // 之后这里的代码就没意义了 如果 execvp 之后的代码被运行了，那就是
    // execvp 出问题了
    exit(255);
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
        auto cmds = vstr_split(args, "|");
        if (!cmds.empty()) {
            int n = cmds.size();
            pid_t pid = fork();
            if (pid == 0) {
                process_out(cmds, 0);
                for (int i = 0; i < n; i++) {
#ifdef TEST
                    pid_t waited_pid = wait(nullptr);
                    std::cerr << getpid() << " wait " << waited_pid
                              << " successfull.\n";
#else
                    wait(nullptr);
#endif
                }
                exit(0);
            } else {
                // 这里只有父进程（原进程）才会进入
                wait(&pid);
            }
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

std::queue<std::vector<std::string>> vstr_split(std::vector<std::string> &vstr,
                                                const std::string &delimiter) {
    std::queue<std::vector<std::string>> ans;
    int last_begin = 0, n = vstr.size();
    for (int i = 0; i < n; i++) {
        if (vstr[i] == delimiter) {
            if (i == last_begin || i == n - 1) {
                std::cout << "Wrong pipe format!" << std::endl;
                while (!ans.empty()) {
                    ans.pop();
                }
                return ans;
            }
            ans.emplace();
            for (int j = last_begin; j < i; j++) {
                ans.back().push_back(vstr[j]);
            }
            last_begin = i + 1;
        }
    }
    ans.emplace();
    for (int j = last_begin; j < n; j++) {
        ans.back().push_back(vstr[j]);
    }
    return ans;
}
