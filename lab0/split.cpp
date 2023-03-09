#include <string>
#include <vector>
#include <iostream>

std::vector<std::string> split(std::string s, const std::string& d){
    std::string t{s};
    auto pos = t.find(d);
    std::vector<std::string> ans;
    while ((pos = t.find(d)) != std::string::npos){
        ans.push_back(t.substr(0, pos));
        t = t.substr(pos + d.length());
    }
    ans.push_back(t);
    return ans;
}

int main(){
    std::string str = "adsf-+qwret-+nvfkbdsj-+orthdfjgh-+dfjrleih";
    std::string delimiter = "-+";
    std::vector<std::string> v = split(str, delimiter);

    for (auto i : v) std::cout << i << std::endl;

    return 0;
}