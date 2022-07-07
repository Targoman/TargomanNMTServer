#include <string>
#include <vector>
#include <tuple>

inline int minofVec(const std::vector<int>& _values)
{
    int min = _values.at(0);
    for(size_t i = 1; i < _values.size(); ++i)
        if(min > _values.at(i))
            min = _values.at(i);
    return min;
}

std::vector<std::tuple<int, int>> getCorrespondence(const std::string& _str1, const std::string& _str2)
{
#define DELETION     0
#define INSERTION    1
#define SUBSTITUTION 2

    const size_t L1 = _str1.size();
    const size_t L2 = _str2.size();
    const size_t S = L1 + 1;

    std::vector<int> D((L1 + 1) * (L2 + 1), 0);
    std::vector<int> Dij(3, 0);

    for(size_t i = 1; i <= L1; ++i)
        D[i] = 4 * i;
    for(size_t i = 1; i <= L2; ++i)
        D[i * S] = 4 * i;
    for(size_t i = 1; i <= L1; ++i) {
        for(size_t j = 1; j <= L2; ++j) {
            Dij[DELETION] = D[j * S + (i - 1)] + 1;
            Dij[INSERTION] = D[(j - 1) * S + i] + 1;
            Dij[SUBSTITUTION] = D[(j - 1) * S + (i - 1)];
            if(_str1.at(i - 1) != _str2.at(j - 1))
                Dij[SUBSTITUTION] += 1;
            D[j * S + i] = minofVec(Dij);
        }
    }

    std::vector<std::tuple<int, int>> correspondence(_str2.size());

    size_t i = L1;
    size_t j = L2;
    size_t end = L1;
    while(i > 0 && j > 0) {
        Dij[DELETION] = D[j * S + (i - 1)] + 1;
        Dij[INSERTION] = D[(j - 1) * S + i] + 1;
        Dij[SUBSTITUTION] = D[(j - 1) * S + (i - 1)];
        if(_str1.at(i - 1) != _str2.at(j - 1))
            Dij[SUBSTITUTION] += 1;

        int winner;
        if((_str1.at(i - 1) == ' ') != (_str2.at(j - 1) == ' ')) {
            if(_str1.at(i - 1) == ' ')
                winner = DELETION;
            else
                winner = INSERTION;
        } else {
            winner = DELETION;
            if(Dij[winner] > Dij[INSERTION])
                winner = INSERTION;
            if(Dij[winner] > Dij[SUBSTITUTION])
                winner = SUBSTITUTION;
        }

        switch(winner) {
        case DELETION:
            --i;
            break;
        case INSERTION:
            correspondence[j - 1] = std::tuple<int, int>(-1, -1);
            --j;
            break;
        case SUBSTITUTION:
            correspondence[j - 1] = std::tuple<int, int>(i - 1, end);
            end = i - 1;
            --i;
            --j;
            break;
        }
    }
    while(j > 0) {
        correspondence[j - 1] = std::tuple<int, int>(-1, -1);
        --j;
    }

    return correspondence;
}

