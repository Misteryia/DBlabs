DROP VIEW IF EXISTS Popular_Products CASCADE;
DROP VIEW IF EXISTS UserOrders CASCADE;
DROP TABLE IF EXISTS Reviews CASCADE;
DROP TABLE IF EXISTS OrderItems CASCADE;
DROP TABLE IF EXISTS Orders CASCADE;
DROP TABLE IF EXISTS Products CASCADE;
DROP TABLE IF EXISTS Categories CASCADE;
DROP TABLE IF EXISTS User_Sessions CASCADE;
DROP TABLE IF EXISTS Users CASCADE;

CREATE TABLE Users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password_hash VARCHAR(64) NOT NULL
);

CREATE TABLE Categories (
    category_id SERIAL PRIMARY KEY,
    name VARCHAR(200) NOT NULL UNIQUE,
    description TEXT
);

CREATE TABLE Products (
    product_id SERIAL PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    description TEXT,
    price DECIMAL(10, 2) NOT NULL,
    stock_quantity INT NOT NULL,
    category_id INT REFERENCES Categories(category_id) ON DELETE CASCADE
);

CREATE TABLE Orders (
    order_id SERIAL PRIMARY KEY,
    user_id INT REFERENCES Users(user_id) ON DELETE CASCADE,
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total_amount DECIMAL(10, 2)
);

CREATE TABLE OrderItems (
    order_item_id SERIAL PRIMARY KEY,
    order_id INT REFERENCES Orders(order_id) ON DELETE CASCADE,
    product_id INT REFERENCES Products(product_id) ON DELETE CASCADE,
    quantity INT NOT NULL,
    price DECIMAL(10, 2) NOT NULL
);

CREATE TABLE Reviews (
    review_id SERIAL PRIMARY KEY,
    product_id INT REFERENCES Products(product_id) ON DELETE CASCADE,
    user_id INT REFERENCES Users(user_id) ON DELETE CASCADE,
    rating INT NOT NULL CHECK (rating >= 1 AND rating <= 5),
    comment TEXT
);

CREATE TABLE User_Sessions (
    session_id SERIAL PRIMARY KEY,
    user_id INT REFERENCES Users(user_id) ON DELETE CASCADE,
    token VARCHAR(60)
);

CREATE OR REPLACE VIEW Popular_Products AS
SELECT p.product_id, p.name, COUNT(o.product_id) AS total_orders
FROM Products p
JOIN OrderItems o ON p.product_id = o.product_id
GROUP BY p.product_id
ORDER BY total_orders DESC;

CREATE OR REPLACE VIEW UserOrders AS
SELECT u.username, o.order_id, o.order_date, o.total_amount
FROM Users u
JOIN Orders o ON u.user_id = o.user_id;

CREATE OR REPLACE FUNCTION update_stock() RETURNS TRIGGER AS $$
BEGIN
    UPDATE Products
    SET stock_quantity = stock_quantity - NEW.quantity
    WHERE product_id = NEW.product_id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE TRIGGER UpdateStock
AFTER INSERT ON OrderItems
FOR EACH ROW
EXECUTE FUNCTION update_stock();

CREATE OR REPLACE FUNCTION CheckStockFunction()
RETURNS TRIGGER 
LANGUAGE plpgsql
AS $$
DECLARE 
    current_stock INT;
BEGIN
    SELECT stock_quantity INTO current_stock 
    FROM Products 
    WHERE product_id = NEW.product_id;

    IF current_stock < NEW.quantity THEN
        RAISE EXCEPTION 'Недостаточно товара на складе';
    END IF;

    RETURN NEW;
END;
$$;

CREATE OR REPLACE TRIGGER CheckStock 
BEFORE INSERT ON OrderItems
FOR EACH ROW
EXECUTE FUNCTION CheckStockFunction();

CREATE OR REPLACE FUNCTION CalculateTotal(order_id INT) RETURNS DECIMAL(10, 2)
LANGUAGE plpgsql
AS $$
DECLARE
    total DECIMAL(10, 2);
BEGIN
    SELECT SUM(quantity * price) INTO total
    FROM OrderItems
    WHERE OrderItems.order_id = CalculateTotal.order_id;

    RETURN total;
END;
$$;

CREATE OR REPLACE PROCEDURE CreateOrder(IN user_id INT, IN product_id INT, IN quantity INT)
LANGUAGE plpgsql
AS $$
DECLARE
    new_order_id INT;
    current_stock INT;
    product_price DECIMAL(10, 2);
BEGIN
    SELECT price, stock_quantity INTO product_price, current_stock
    FROM Products
    WHERE Products.product_id = CreateOrder.product_id;

    IF product_price IS NULL THEN
        RAISE EXCEPTION 'Товара с таким артикулом не существует';
    END IF;

    IF current_stock < quantity THEN
        RAISE EXCEPTION 'Недостаточно товара на складе';
    END IF;

    INSERT INTO Orders (user_id, order_date, total_amount)
    VALUES (user_id, NOW(), 0)
    RETURNING order_id INTO new_order_id;

    INSERT INTO OrderItems (order_id, product_id, quantity, price)
    VALUES (new_order_id, CreateOrder.product_id, quantity, product_price);

    UPDATE Orders
    SET total_amount = CalculateTotal(new_order_id)
    WHERE Orders.order_id = new_order_id;

EXCEPTION
    WHEN others THEN
        RAISE EXCEPTION '%', SQLERRM;
END;
$$;

CREATE OR REPLACE FUNCTION CheckReviewUnique()
RETURNS TRIGGER 
LANGUAGE plpgsql
AS $$
BEGIN
    IF EXISTS (SELECT 1 FROM Reviews WHERE user_id = NEW.user_id AND product_id = NEW.product_id) THEN
        RAISE EXCEPTION 'Вы уже оставляли отзыв на этот товар';
    END IF;
    RETURN NEW;
END;
$$;

CREATE OR REPLACE TRIGGER EnsureReviewUnique
BEFORE INSERT ON Reviews
FOR EACH ROW
EXECUTE FUNCTION CheckReviewUnique();

INSERT INTO Categories (name, description) VALUES
    ('Белый чай', 'Лучший легкий чай.'),
    ('Зелёный чай', 'Лёгкий и освежающий зелёный чай.'),
    ('Жёлтый чай', 'Классический желтый чай.'),
    ('Чай улун', 'Лучший улун на свете!'),
    ('Чёрный чай', 'Классический чёрный чай с насыщенным вкусом.'),
    ('Красный чай', 'Расслабляющий красный чай'),
    ('Чай пуэр', 'Тут всё понятно и так.');

ALTER TABLE User_Sessions OWNER TO dbowner;
ALTER TABLE Reviews OWNER TO dbowner;
ALTER TABLE OrderItems OWNER TO dbowner;
ALTER TABLE Orders OWNER TO dbowner;
ALTER TABLE Products OWNER TO dbowner;
ALTER TABLE Categories OWNER TO dbowner;
ALTER TABLE Users OWNER TO dbowner;
ALTER VIEW Popular_Products OWNER TO dbowner;
ALTER VIEW UserOrders OWNER TO dbowner;