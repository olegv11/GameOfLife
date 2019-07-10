#include <iostream>
#include <sstream>
#include <vector>
#include <random>
#include <chrono>
#include <string.h>

class Life
{
public:
    Life(int height, int width);
    void Fill(double fillRate);
    
    void AdvanceGeneration();
    
    void SetStateFront(int x, int y, int state);
    void PlaceGlider(int x, int y);
    void Print() const;
    
private:
    void SetStateBack(int x, int y, int state);
    __attribute__((always_inline)) inline int At(int x, int y) const;
    int CountNeighbours(int x, int y) const;
    std::pair<int, int> Wrap(int x, int y) const;
    
    int m_height;
    int m_width;
    std::vector<char> m_field;
    std::vector<char> m_backField;
    int m_stride;
};

Life::Life(int height, int width)
: m_height(height), m_width(width)
{
    // Ячейки храним как 1 char. 1 = жив, 0 = мёртв
    // Вокруг поля будет пэддинг размером 1, содержащий в себе нули. Таким образом у нас конечное поле и можно при подсчёте числа соседей не проверять, 
    // находимся ли мы на границе. Если на каждом шаге в пэддинг копировать значение с противоположного конца поля, получится тороидальное поле. 
    int bytes = (height + 2) * (width + 2);
    m_stride = width + 2;
    m_field.resize(bytes);
    m_backField.resize(bytes);
    
    memset(m_field.data(), 0, bytes);
    memset(m_backField.data(), 0, bytes);
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
    for (int y = 0; y < m_height; y++)
    {
        for (int x = 0; x < m_width; x++)
        {
            int currentCell = At(x, y);
            int neighbours = CountNeighbours(x, y);
            if (neighbours == 3 || (currentCell == 1 && neighbours == 2))
            {
                SetStateBack(x, y, 1);
            }
            else
            {
                SetStateBack(x, y, 0);
            }
        }
    }
    std::swap(m_field, m_backField);
}

__attribute__((always_inline)) int Life::At(int x, int y) const
{
    int pos = (y + 1) * m_stride + (x + 1);
    return m_field[pos];
}

void Life::SetStateBack(int x, int y, int state)
{
    int pos = (y + 1) * m_stride + (x + 1);
    m_backField[pos] = state;
}

void Life::SetStateFront(int x, int y, int state)
{
    int pos = (y + 1) * m_stride + (x + 1);
    m_field[pos] = state;
}

//
// -*-
// --*
// ***

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
            if (At(x,y) == 1)
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

int Life::CountNeighbours(int x, int y) const
{
    int leftx = x - 1, rightx = x + 1;
    int upy = y - 1, downy = y + 1;
    
    return At(leftx, upy)
    + At(x, upy)
    + At(rightx, upy)
    + At(rightx, y)
    + At(rightx, downy)
    + At(x, downy)
    + At(leftx, downy)
    + At(leftx, y);
}

std::pair<int, int> Life::Wrap(int x, int y) const
{
    if (x == m_width) x = 0;
    if (x == -1) x = m_width - 1;
    if (y == m_height) y = 0;
    if (y == -1) y = m_height - 1;
    
    return std::make_pair(x, y);
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


