#include <pqxx/pqxx>
#include <random>
#include <vector>
#include <string>
#include <fstream>
#include <tuple>
#include <iostream>

// Функция для получения соединения
pqxx::connection get_connection() {
    std::string connection_string = "postgresql://dbowner:1234@localhost:5432/five_millions";
    return pqxx::connection(connection_string);
}

int main() {
    std::vector<std::string> categories = {"Белый чай", "Зелёный чай", "Жёлтый чай", "Чай улун", "Чёрный чай", "Красный чай", "Чай пуэр"};
    std::vector<std::string> teaForms = {"листовой", "пакетированный", "гранулированный", "прессованный", "порошковый", "цветочный"};
    std::vector<std::string> teaBrands = {"Twinings", "Lipton", "Fortnum & Mason", "Harney & Sons", "Ahmad Tea", "Dilmah", "Taj Mahal", "Aromistico", "Майский чай", "Принцесса Нури", "Greenfield", "TeaGschwendner", "Lupicia", "ITO EN", "Tazo", "Lipton", "Ahmad Tea", "Teavana", "Ten Ren", "Eco-Cha", "Menghai Tea Factory", "Xiaguan", "Yunnan Sourcing", "Celestial Seasonings", "Yogi Tea", "Pukka Herbs", "Mariage Frères", "Wedgwood", "Rare Tea Company"};
    std::vector<std::string> weights = {"10г", "50г", "100г", "150г", "200г", "250г", "300г", "500г", "700г", "1000г"};
    std::vector<std::string> d1 = {"ограниченая версия", "лимитированая версия", "премиум версия", "подарочная версия", "коллекционная версия", "праздничная версия"};
    std::vector<std::string> packaging = {"жестяная банка", "картонная коробка", "керамическая банка", "деревянная шкатулка", "тканевый мешочек", "фольгированный пакет", "стеклянная банка"};
    std::vector<std::string> origins = {"Китай", "Индия", "Япония", "Шри-Ланка", "Кения", "Вьетнам", "Тайвань", "Непал", "Россия", "Грузия", "Иран", "Турция"};

    std::vector<std::string> years;
    for (int year = 1930; year <= 2024; ++year) {
        years.push_back(std::to_string(year));
    }

    // Инициализация генератора случайных чисел
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 100000000);

    pqxx::connection conn = get_connection();

    // Оптимизация 1: Настройка соединения (отключаем синхронный коммит)
    {
        pqxx::work setup(conn);
        setup.exec("SET synchronous_commit TO OFF;");
        setup.commit();
    }

    // Оптимизация 2: Подготовленное выражение для пакетной вставки
    conn.prepare("insert_batch",
        "INSERT INTO Products (name, description, price, stock_quantity, category_id) "
        "VALUES ($1, $2, $3, $4, $5)");

    const int batch_size = 25000; // Размер пакета
    
    for (int i = 0; i < 5500000; ++i) {
        // Создаем новую транзакцию для каждого пакета
        if (i % batch_size == 0) {
            pqxx::work txn(conn);
            
            // Вставляем batch_size записей в одной транзакции
            for (int j = 0; j < batch_size && i + j < 5500000; ++j) {
                int category_id = dist(gen) % 7;
                std::string name = categories[category_id] + " " + 
                                teaForms[dist(gen) % teaForms.size()] + " " + 
                                teaBrands[dist(gen) % teaBrands.size()] + " " + 
                                d1[dist(gen) % d1.size()] + ". Упаковка: " + 
                                packaging[dist(gen) % packaging.size()] + ". Вес: " + 
                                weights[dist(gen) % weights.size()] + ". Страна: " + 
                                origins[dist(gen) % origins.size()] + ". Год производства: " + 
                                years[dist(gen) % years.size()] + ".";

                txn.exec_prepared("insert_batch", 
                                name, 
                                "-", 
                                dist(gen) % 1000, 
                                dist(gen) % 100, 
                                category_id + 1);
            }
            
            txn.commit();
            
            if (i % 100000 == 0) {
                std::cout << "Processed " << i << " records..." << std::endl;
            }
        }
    }

    std::cout << "Data insertion completed successfully!" << std::endl;
    return 0;
}