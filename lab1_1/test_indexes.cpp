#include <pqxx/pqxx>
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

pqxx::connection get_connection() {
    std::string connection_string = "postgresql://dbowner:1234@localhost:5432/five_millions";
    return pqxx::connection(connection_string);
}

// Выполнение запроса и время выполнения
double execute_and_time(pqxx::connection &conn, const std::string &query) {
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        pqxx::work txn(conn);
        pqxx::result result = txn.exec(query);
        txn.commit();
    } catch (const std::exception &e) {
        std::cerr << "Error in query: " << e.what() << std::endl;
        return -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    return duration.count();
}

// Перевод времени
std::string format_duration(double seconds) {
    std::ostringstream oss;
    if (seconds < 0.001) {
        oss << std::fixed << std::setprecision(2) << seconds * 1000000 << " μs";
    } else if (seconds < 1.0) {
        oss << std::fixed << std::setprecision(2) << seconds * 1000 << " ms";
    } else {
        oss << std::fixed << std::setprecision(2) << seconds << " s";
    }
    return oss.str();
}

// Тест индексов
void test_indexes() {
    pqxx::connection conn = get_connection();
    
    // Запросы
    std::vector<std::pair<std::string, std::string>> queries = {
        // B-tree тесты
        {"B-tree: Точечный поиск по цене", "SELECT * FROM Products WHERE price = 500 LIMIT 100;"},
        {"B-tree: Диапазон цен", "SELECT * FROM Products WHERE price BETWEEN 100 AND 200 LIMIT 100;"},
        {"B-tree: Сортировка по цене", "SELECT * FROM Products ORDER BY price DESC LIMIT 100;"},
        
        // GIN тесты
        {"GIN: Поиск слова в любом месте текста", "SELECT * FROM Products WHERE name LIKE '%чай%' LIMIT 100;"},
        {"GIN: Поиск фразы", "SELECT * FROM Products WHERE name LIKE '%зелёный чай%' LIMIT 100;"},
        {"GIN: Поиск с использованием триграмм", "SELECT * FROM Products WHERE name ~ '\\yзеленый\\y' LIMIT 100;"},
        
        // BRIN тесты
        {"BRIN: Диапазон по последовательным значениям цены", "SELECT * FROM Products WHERE price BETWEEN 100 AND 200 LIMIT 100;"},
        {"BRIN: Агрегация по категориям", "SELECT category_id, AVG(price) FROM Products GROUP BY category_id;"},
        {"BRIN: Диапазон по упорядоченным данным", "SELECT * FROM Products WHERE product_id BETWEEN 1000000 AND 1000100;"}
    };
    
    // Тестирование
    std::cout << "=== Тестирование без индексов ===" << std::endl;
    std::vector<double> times;
    for (const auto &q : queries) {
        double time = execute_and_time(conn, q.second);
        times.push_back(time);
        std::cout << q.first << ": " << format_duration(time) << std::endl;
    }
    
    // Создание индексов
    std::vector<std::pair<std::string, std::string>> indexes = {
        {"B-tree индекс на price", "CREATE INDEX idx_products_price ON Products USING btree(price);"},
        {"GIN индекс на name (триграммы)", "CREATE INDEX idx_products_name_gin ON Products USING gin(name gin_trgm_ops);"},
        {"BRIN индекс на product_id", "CREATE INDEX idx_products_id_brin ON Products USING brin(product_id);"}
    };
    
    // Тест с каждым индексом
    for (size_t i = 0; i < indexes.size(); ++i) {
        std::cout << indexes[i].first << " ===" << std::endl;
        
        // Создаем индекс
        {
            pqxx::work txn(conn);
            txn.exec(indexes[i].second);
            txn.commit();
        }
        
        // Тестируем запросы
        for (size_t q_idx = 0; q_idx < queries.size(); ++q_idx) {
            bool relevant = false;
            if (i == 0 && (q_idx == 0 || q_idx == 1 || q_idx == 2)) relevant = true; // B-tree price
            if (i == 1 && (q_idx == 3 || q_idx == 4 || q_idx == 5)) relevant = true; // GIN
            if (i == 2 && (q_idx == 6 || q_idx == 7 || q_idx == 8)) relevant = true; // BRIN
            
            if (relevant) {
                double time = execute_and_time(conn, queries[q_idx].second);
                double speedup = times[q_idx] / time;
                std::cout << queries[q_idx].first << ": " << format_duration(time) 
                          << " (ускорение в " << std::fixed << std::setprecision(2) << speedup << "x)" << std::endl;
            }
        }
        
        // Удаляем индекс
        {
            pqxx::work txn(conn);
            txn.exec("DROP INDEX " + std::string(
                i == 0 ? "idx_products_price" :
                i == 1 ? "idx_products_name_gin" : "idx_products_id_brin") + ";");
            txn.commit();
        }
    }
    
    std::cout << "\n=== Тестирование всех индексов одновременно ===" << std::endl;
    
    {
        pqxx::work txn(conn);
        for (const auto &idx : indexes) {
            txn.exec(idx.second);
        }
        txn.commit();
    }
    
    for (size_t q_idx = 0; q_idx < queries.size(); ++q_idx) {
        double time = execute_and_time(conn, queries[q_idx].second);
        double speedup = times[q_idx] / time;
        std::cout << queries[q_idx].first << ": " << format_duration(time) 
                  << " (ускорение в " << std::fixed << std::setprecision(2) << speedup << "x)" << std::endl;
    }
    
    {
        pqxx::work txn(conn);
        txn.exec("DROP INDEX idx_products_price;");
        txn.exec("DROP INDEX idx_products_name_gin;");
        txn.exec("DROP INDEX idx_products_id_brin;");
        txn.commit();
    }
}

int main() {
    try {
        test_indexes();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
