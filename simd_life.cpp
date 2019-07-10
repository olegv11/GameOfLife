#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <stdlib.h>
#include <string.h>
#include <chrono>

#include <emmintrin.h>

class Life
{
public:
    Life(int height, int width);
    void Fill(double fillRate);
    
    ~Life();
    void AdvanceGeneration();

    void SetStateFront(int x, int y, int state);
    void PlaceGlider(int x, int y);
    void Print() const;

private:
    void SetStateBack(int x, int y, int state);
    int At(int x, int y) const;
    int m_height;
    int m_width;
    
    uint8_t *m_field;
    uint8_t *m_backField;
    int m_stride;
};

Life::Life(int height, int width)
    : m_height(height), m_width(width)
{
    // На 1 ячейку используем 1 char. 
    // Вокруг поля будет пэддинг размером 1, содержащий в себе нули. Таким образом у нас конечное поле и можно при подсчёте числа соседей не проверять, 
    // находимся ли мы на границе. Если на каждом шаге в пэддинг копировать значение с противоположного конца поля, получится тороидальное поле. 
    // Также для SIMD-загрузки необходимы строки, кратные размерам XMM регистров, поэтому сделаем размер выделяемой строки, кратный 32 байтам.
    int heightPlusPad = height + 2;
    int widthPlusPad = width + 2;
    m_stride = (widthPlusPad + 31) / 32 * 32;

    m_field = (uint8_t*)aligned_alloc(32, m_stride * heightPlusPad);
    m_backField = (uint8_t*)aligned_alloc(32, m_stride * heightPlusPad);

    memset(m_field, 0, m_stride * heightPlusPad);
    memset(m_backField, 0, m_stride * heightPlusPad);
}

Life::~Life()
{
    free(m_field);
    free(m_backField);
}


void Life::Fill(double fillRate)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::discrete_distribution<> d({ 1.0 - fillRate, fillRate });

    for (int y = 0; y < m_height + 2; y++)
    {
        for (int x = 0; x < m_width + 2; x++)
        {
            int cell = y * m_stride + x;
            if (y == 0 || y == m_height + 1)
            {
                m_field[cell] = 0;
            }
            else if (x == 0 || x == m_width + 1)
            {
                m_field[cell] = 0;
            }
            else
            {
                m_field[cell] = d(gen);
            }
        }
    }
}

void Life::AdvanceGeneration()
{
    __m128i zeroi = _mm_set1_epi8(0);
    __m128i onei = _mm_set1_epi8(1);
    __m128i twoi = _mm_set1_epi8(2);
    __m128i threei = _mm_set1_epi8(3);

    __m128i maxXIndices = _mm_set1_epi8(m_width);
    __m128i startXIndices = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __m128i xIncrement = _mm_set1_epi8(16);
    
    uint8_t *prevLine = m_field;
    uint8_t *currentLine = m_field + m_stride;
    uint8_t *nextLine = m_field + 2 * m_stride;

    uint8_t *destination = m_backField + m_stride + 1;


    for (int y = 0; y < m_height; y++)
    {
        __m128i xIndices = startXIndices;
        for (int x = 0; x < m_width; x += 16)
        {
            __m128i ul = _mm_loadu_si128((__m128i*)(prevLine + x)), uu = _mm_loadu_si128((__m128i*)(prevLine + x + 1)), ur = _mm_loadu_si128((__m128i*)(prevLine + x + 2));
            __m128i l = _mm_loadu_si128((__m128i*)(currentLine + x)), c = _mm_loadu_si128((__m128i*)(currentLine + x + 1)), r = _mm_loadu_si128((__m128i*)(currentLine + x + 2));
            __m128i dl = _mm_loadu_si128((__m128i*)(nextLine + x)), dd = _mm_loadu_si128((__m128i*)(nextLine + x + 1)), dr = _mm_loadu_si128((__m128i*)(nextLine + x + 2));

#define ADD(a, b) _mm_add_epi8((a), (b))
            __m128i neighbours = ADD(ADD(ADD(ul, uu), ADD(ur, l)), ADD(ADD(r, dl), ADD(dd, dr)));
#undef ADD

            // Клетка будет живой на следующей итерации если:
            // 1) Она сейчас жива и у неё два соседа
            __m128i twoNeighbours = _mm_cmpeq_epi8(neighbours, twoi);
            __m128i currentAlive = _mm_cmpeq_epi8(c, onei);
            __m128i aliveAndTwoNeighbours = _mm_and_si128(currentAlive, twoNeighbours);
            // 2) У неё три соседа (не важно, жива или нет)
            __m128i threeNeighbours = _mm_cmpeq_epi8(neighbours, threei);

            __m128i willBeAlive = _mm_or_si128(threeNeighbours, aliveAndTwoNeighbours);

            // Так как идём по 16 элементов, мы можем вылезти на пэддинг, который всегда должен оставаться нулём,
            // поэтому занулим выскакивающие элементы
            __m128i writeMask = _mm_cmplt_epi8(xIndices, maxXIndices);
            willBeAlive = _mm_and_si128(willBeAlive, writeMask);

            __m128i result = _mm_and_si128(willBeAlive, onei);
            _mm_storeu_si128((__m128i*)(destination + x), result);

            xIndices = _mm_adds_epi8(xIndices, xIncrement);            
        }

        prevLine += m_stride;
        currentLine += m_stride;
        nextLine += m_stride;
        destination += m_stride;

    }
    std::swap(m_field, m_backField);
}

int Life::At(int x, int y) const
{
    int pos = (y + 1) * m_stride + (x + 1);
    return m_field[pos];
}

void Life::SetStateBack(int x, int y, int state)
{
    int pos = (y + 1) * m_stride + (x + 1);
    if (state == 1)
    {
        m_backField[pos] = 1;
    }
    else
    {
        m_backField[pos] = 0;
    }
}

void Life::SetStateFront(int x, int y, int state)
{
    int pos = (y + 1) * m_stride + (x + 1);
    if (state == 1)
    {
        m_field[pos] = 1;
    }
    else
    {
        m_field[pos] = 0;
    }
}

/*
-*-
--*
***
*/
void Life::PlaceGlider(int x, int y)
{
    SetStateFront(x + 1, y, 1);
    SetStateFront(x + 2, y + 1, 1);
    SetStateFront(x, y + 2, 1);
    SetStateFront(x + 1, y + 2, 1);
    SetStateFront(x + 2, y + 2, 1);
}

void Life::Print() const
{
    for (int y = 0; y < m_height; y++)
    {
        for (int x = 0; x < m_width; x++)
        {
            if (At(x, y) != 0)
            {
                std::cout << '*';
            }
            else
            {
                std::cout << '-';
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n\n";
}

int main(int argc, char* argv[])
{
    if (argc != 6)
    {
        std::cout << "Usage: simple_life width height generations fill_rate print\n";
        return 1;
    }
    
    int width, height, generations;
    double fillRate;
    bool print;
    try
    {
        width = std::stoi(argv[1]);
        height = std::stoi(argv[2]);
        generations = std::stoi(argv[3]);
        fillRate = std::stod(argv[4]);
        print = std::stoi(argv[5]) != 0;
    }
    catch (...)
    {
        std::cout << "Error processing parameters, terminating\n";
        return 1;
    }
    
    Life life(height, width);
    if (fillRate == 0)
    {
        life.PlaceGlider(0, 0);
    }
    else
    {
        life.Fill(fillRate);
    }
    
    if (print)
        life.Print();
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < generations; i++)
    {
        life.AdvanceGeneration();
        if (print)
            life.Print();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Time per generation:" << milliseconds / generations << "ms" << std::endl;
    
    return 0;
}
