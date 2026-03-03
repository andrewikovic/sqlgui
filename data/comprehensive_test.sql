PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;
PRAGMA temp_store = MEMORY;
PRAGMA cte_max_recursion_depth = 100000;

BEGIN IMMEDIATE;

DROP VIEW IF EXISTS v_support_ticket_backlog;
DROP VIEW IF EXISTS v_top_selling_products;
DROP VIEW IF EXISTS v_inventory_alerts;
DROP VIEW IF EXISTS v_customer_lifetime_value;
DROP VIEW IF EXISTS v_order_summary;

DROP TABLE IF EXISTS ticket_comments;
DROP TABLE IF EXISTS support_tickets;
DROP TABLE IF EXISTS shipments;
DROP TABLE IF EXISTS payments;
DROP TABLE IF EXISTS order_items;
DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS inventory;
DROP TABLE IF EXISTS warehouses;
DROP TABLE IF EXISTS documents;
DROP TABLE IF EXISTS products;
DROP TABLE IF EXISTS suppliers;
DROP TABLE IF EXISTS categories;
DROP TABLE IF EXISTS addresses;
DROP TABLE IF EXISTS customers;
DROP TABLE IF EXISTS feature_flags;
DROP TABLE IF EXISTS metrics_daily;
DROP TABLE IF EXISTS events;
DROP TABLE IF EXISTS audit_log;
DROP TABLE IF EXISTS user_roles;
DROP TABLE IF EXISTS roles;
DROP TABLE IF EXISTS users;
DROP TABLE IF EXISTS organizations;

CREATE TABLE organizations (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    industry TEXT NOT NULL,
    tier TEXT NOT NULL,
    status TEXT NOT NULL,
    created_at TEXT NOT NULL,
    settings_json TEXT NOT NULL
);

CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    organization_id INTEGER NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    email TEXT NOT NULL UNIQUE,
    full_name TEXT NOT NULL,
    title TEXT NOT NULL,
    locale TEXT NOT NULL DEFAULT 'en_US',
    timezone TEXT NOT NULL DEFAULT 'UTC',
    is_active INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    last_login_at TEXT,
    profile_json TEXT NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE roles (
    id INTEGER PRIMARY KEY,
    code TEXT NOT NULL UNIQUE,
    description TEXT NOT NULL
);

CREATE TABLE user_roles (
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    role_id INTEGER NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
    granted_at TEXT NOT NULL,
    PRIMARY KEY (user_id, role_id)
) WITHOUT ROWID;

CREATE TABLE customers (
    id INTEGER PRIMARY KEY,
    organization_id INTEGER NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    customer_code TEXT NOT NULL UNIQUE,
    company_name TEXT NOT NULL,
    contact_name TEXT NOT NULL,
    email TEXT,
    phone TEXT,
    segment TEXT NOT NULL,
    credit_limit NUMERIC NOT NULL,
    balance NUMERIC NOT NULL DEFAULT 0,
    is_active INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    notes TEXT,
    created_at TEXT NOT NULL
);

CREATE TABLE addresses (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
    address_type TEXT NOT NULL,
    line1 TEXT NOT NULL,
    line2 TEXT,
    city TEXT NOT NULL,
    state_code TEXT NOT NULL,
    postal_code TEXT NOT NULL,
    country_code TEXT NOT NULL DEFAULT 'US',
    is_default INTEGER NOT NULL DEFAULT 0 CHECK (is_default IN (0, 1)),
    created_at TEXT NOT NULL
);

CREATE TABLE categories (
    id INTEGER PRIMARY KEY,
    parent_id INTEGER REFERENCES categories(id),
    name TEXT NOT NULL UNIQUE,
    slug TEXT NOT NULL UNIQUE
);

CREATE TABLE suppliers (
    id INTEGER PRIMARY KEY,
    organization_id INTEGER NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    supplier_code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    email TEXT,
    phone TEXT,
    lead_time_days INTEGER NOT NULL,
    rating NUMERIC NOT NULL,
    is_preferred INTEGER NOT NULL DEFAULT 0 CHECK (is_preferred IN (0, 1)),
    created_at TEXT NOT NULL
);

CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    category_id INTEGER NOT NULL REFERENCES categories(id),
    supplier_id INTEGER NOT NULL REFERENCES suppliers(id),
    sku TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    description TEXT NOT NULL,
    unit_price NUMERIC NOT NULL,
    unit_cost NUMERIC NOT NULL,
    status TEXT NOT NULL,
    weight_kg NUMERIC NOT NULL,
    attributes_json TEXT NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE warehouses (
    id INTEGER PRIMARY KEY,
    organization_id INTEGER NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    city TEXT NOT NULL,
    region TEXT NOT NULL,
    capacity_units INTEGER NOT NULL,
    is_active INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1))
);

CREATE TABLE inventory (
    id INTEGER PRIMARY KEY,
    product_id INTEGER NOT NULL REFERENCES products(id) ON DELETE CASCADE,
    warehouse_id INTEGER NOT NULL REFERENCES warehouses(id) ON DELETE CASCADE,
    quantity_on_hand INTEGER NOT NULL,
    reserved_quantity INTEGER NOT NULL DEFAULT 0,
    reorder_point INTEGER NOT NULL,
    last_counted_at TEXT NOT NULL,
    UNIQUE (product_id, warehouse_id)
);

CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL REFERENCES customers(id),
    sales_rep_user_id INTEGER REFERENCES users(id),
    order_number TEXT NOT NULL UNIQUE,
    status TEXT NOT NULL,
    order_date TEXT NOT NULL,
    required_date TEXT NOT NULL,
    shipped_date TEXT,
    currency_code TEXT NOT NULL DEFAULT 'USD',
    subtotal NUMERIC NOT NULL DEFAULT 0,
    tax_amount NUMERIC NOT NULL DEFAULT 0,
    shipping_amount NUMERIC NOT NULL DEFAULT 0,
    discount_amount NUMERIC NOT NULL DEFAULT 0,
    total_amount NUMERIC NOT NULL DEFAULT 0,
    source TEXT NOT NULL,
    notes TEXT,
    updated_at TEXT NOT NULL
);

CREATE TABLE order_items (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES orders(id) ON DELETE CASCADE,
    product_id INTEGER NOT NULL REFERENCES products(id),
    line_number INTEGER NOT NULL,
    quantity INTEGER NOT NULL,
    unit_price NUMERIC NOT NULL,
    discount_percent NUMERIC NOT NULL,
    tax_percent NUMERIC NOT NULL,
    line_total NUMERIC NOT NULL,
    UNIQUE (order_id, line_number)
);

CREATE TABLE payments (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES orders(id) ON DELETE CASCADE,
    payment_reference TEXT NOT NULL UNIQUE,
    paid_at TEXT NOT NULL,
    amount NUMERIC NOT NULL,
    method TEXT NOT NULL,
    status TEXT NOT NULL,
    processor_response_json TEXT NOT NULL
);

CREATE TABLE shipments (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES orders(id) ON DELETE CASCADE,
    warehouse_id INTEGER NOT NULL REFERENCES warehouses(id),
    tracking_number TEXT NOT NULL UNIQUE,
    carrier TEXT NOT NULL,
    shipped_at TEXT NOT NULL,
    delivered_at TEXT,
    status TEXT NOT NULL,
    shipping_cost NUMERIC NOT NULL
);

CREATE TABLE support_tickets (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
    owner_user_id INTEGER REFERENCES users(id),
    priority TEXT NOT NULL,
    status TEXT NOT NULL,
    subject TEXT NOT NULL,
    description TEXT NOT NULL,
    opened_at TEXT NOT NULL,
    closed_at TEXT,
    tags_json TEXT NOT NULL
);

CREATE TABLE ticket_comments (
    id INTEGER PRIMARY KEY,
    ticket_id INTEGER NOT NULL REFERENCES support_tickets(id) ON DELETE CASCADE,
    author_user_id INTEGER REFERENCES users(id),
    body TEXT NOT NULL,
    created_at TEXT NOT NULL,
    is_internal INTEGER NOT NULL DEFAULT 0 CHECK (is_internal IN (0, 1))
);

CREATE TABLE feature_flags (
    id INTEGER PRIMARY KEY,
    organization_id INTEGER NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    flag_key TEXT NOT NULL,
    enabled INTEGER NOT NULL CHECK (enabled IN (0, 1)),
    rollout_percent INTEGER NOT NULL CHECK (rollout_percent BETWEEN 0 AND 100),
    rules_json TEXT NOT NULL,
    UNIQUE (organization_id, flag_key)
);

CREATE TABLE documents (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER REFERENCES customers(id) ON DELETE SET NULL,
    file_name TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    size_bytes INTEGER NOT NULL,
    checksum TEXT NOT NULL,
    content_blob BLOB NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE audit_log (
    id INTEGER PRIMARY KEY,
    actor_user_id INTEGER REFERENCES users(id),
    entity_type TEXT NOT NULL,
    entity_id INTEGER NOT NULL,
    action TEXT NOT NULL,
    changed_at TEXT NOT NULL,
    ip_address TEXT NOT NULL,
    metadata_json TEXT NOT NULL,
    payload_blob BLOB
);

CREATE TABLE metrics_daily (
    organization_id INTEGER NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    metric_date TEXT NOT NULL,
    active_users INTEGER NOT NULL,
    new_customers INTEGER NOT NULL,
    order_count INTEGER NOT NULL,
    gross_revenue NUMERIC NOT NULL,
    return_rate NUMERIC NOT NULL,
    PRIMARY KEY (organization_id, metric_date)
) WITHOUT ROWID;

CREATE TABLE events (
    id INTEGER PRIMARY KEY,
    stream_name TEXT NOT NULL,
    event_type TEXT NOT NULL,
    aggregate_id TEXT NOT NULL,
    sequence_no INTEGER NOT NULL,
    published_at TEXT NOT NULL,
    payload_json TEXT NOT NULL,
    correlation_id TEXT NOT NULL,
    UNIQUE (stream_name, aggregate_id, sequence_no)
);

INSERT INTO organizations (id, name, industry, tier, status, created_at, settings_json)
VALUES
    (1, 'Acorn Retail', 'retail', 'enterprise', 'active', '2020-01-10T08:00:00Z', '{"region":"na","multiWarehouse":true,"alerts":"verbose"}'),
    (2, 'Bluebird Health', 'healthcare', 'growth', 'active', '2020-03-18T08:00:00Z', '{"region":"na","multiWarehouse":false,"alerts":"normal"}'),
    (3, 'Cinder Logistics', 'logistics', 'enterprise', 'active', '2020-05-02T08:00:00Z', '{"region":"eu","multiWarehouse":true,"alerts":"verbose"}'),
    (4, 'Delta Foods', 'food', 'growth', 'active', '2020-06-22T08:00:00Z', '{"region":"na","multiWarehouse":true,"alerts":"normal"}'),
    (5, 'Evergreen Media', 'media', 'starter', 'active', '2020-07-14T08:00:00Z', '{"region":"na","multiWarehouse":false,"alerts":"quiet"}'),
    (6, 'Foundry Systems', 'manufacturing', 'enterprise', 'active', '2020-09-03T08:00:00Z', '{"region":"apac","multiWarehouse":true,"alerts":"verbose"}'),
    (7, 'Granite Telecom', 'telecom', 'growth', 'active', '2020-10-29T08:00:00Z', '{"region":"eu","multiWarehouse":true,"alerts":"normal"}'),
    (8, 'Harbor Travel', 'travel', 'starter', 'active', '2021-01-11T08:00:00Z', '{"region":"na","multiWarehouse":false,"alerts":"quiet"}'),
    (9, 'Iron Orchard', 'agriculture', 'growth', 'active', '2021-02-17T08:00:00Z', '{"region":"na","multiWarehouse":true,"alerts":"normal"}'),
    (10, 'Juniper Finance', 'finance', 'enterprise', 'active', '2021-03-23T08:00:00Z', '{"region":"na","multiWarehouse":false,"alerts":"verbose"}'),
    (11, 'Keystone Energy', 'energy', 'enterprise', 'active', '2021-05-07T08:00:00Z', '{"region":"eu","multiWarehouse":true,"alerts":"verbose"}'),
    (12, 'Lumen Education', 'education', 'starter', 'trial', '2021-06-01T08:00:00Z', '{"region":"na","multiWarehouse":false,"alerts":"quiet"}');

INSERT INTO roles (id, code, description)
VALUES
    (1, 'admin', 'Full administrative access'),
    (2, 'analyst', 'Read-heavy analytical access'),
    (3, 'operator', 'Operational updates and fulfillment'),
    (4, 'support', 'Customer support workflows'),
    (5, 'viewer', 'Read-only access');

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 600
)
INSERT INTO users (
    id,
    organization_id,
    email,
    full_name,
    title,
    locale,
    timezone,
    is_active,
    last_login_at,
    profile_json,
    created_at
)
SELECT
    n,
    ((n - 1) % 12) + 1,
    printf('user%04d@example.test', n),
    printf('User %04d', n),
    CASE n % 5
        WHEN 0 THEN 'Sales Manager'
        WHEN 1 THEN 'Operations Lead'
        WHEN 2 THEN 'Support Specialist'
        WHEN 3 THEN 'Data Analyst'
        ELSE 'Account Executive'
    END,
    CASE n % 3
        WHEN 0 THEN 'en_US'
        WHEN 1 THEN 'en_GB'
        ELSE 'fr_FR'
    END,
    CASE n % 4
        WHEN 0 THEN 'UTC'
        WHEN 1 THEN 'America/New_York'
        WHEN 2 THEN 'Europe/London'
        ELSE 'Asia/Singapore'
    END,
    CASE WHEN n % 17 = 0 THEN 0 ELSE 1 END,
    datetime('2024-12-31T12:00:00Z', printf('-%d hours', (n * 7) % 1440)),
    printf('{"theme":"%s","dashboard":"%s","alerts":%d}', CASE WHEN n % 2 = 0 THEN 'light' ELSE 'dark' END, CASE WHEN n % 3 = 0 THEN 'ops' ELSE 'sales' END, n % 5),
    datetime('2021-01-01T08:00:00Z', printf('+%d days', n % 720))
FROM seq;

INSERT INTO user_roles (user_id, role_id, granted_at)
SELECT id, 5, datetime(created_at, '+1 day') FROM users;

INSERT INTO user_roles (user_id, role_id, granted_at)
SELECT id, 1, datetime(created_at, '+2 days') FROM users WHERE id % 40 = 0;

INSERT INTO user_roles (user_id, role_id, granted_at)
SELECT id, 2, datetime(created_at, '+3 days') FROM users WHERE id % 6 = 0;

INSERT INTO user_roles (user_id, role_id, granted_at)
SELECT id, 3, datetime(created_at, '+4 days') FROM users WHERE id % 4 = 0;

INSERT INTO user_roles (user_id, role_id, granted_at)
SELECT id, 4, datetime(created_at, '+5 days') FROM users WHERE id % 7 = 0;

INSERT INTO categories (id, parent_id, name, slug)
VALUES
    (1, NULL, 'Hardware', 'hardware'),
    (2, NULL, 'Software', 'software'),
    (3, NULL, 'Services', 'services'),
    (4, 1, 'Laptops', 'hardware-laptops'),
    (5, 1, 'Desktops', 'hardware-desktops'),
    (6, 1, 'Networking', 'hardware-networking'),
    (7, 2, 'Analytics', 'software-analytics'),
    (8, 2, 'Security', 'software-security'),
    (9, 2, 'Productivity', 'software-productivity'),
    (10, 3, 'Consulting', 'services-consulting'),
    (11, 3, 'Training', 'services-training'),
    (12, 3, 'Support Plans', 'services-support'),
    (13, 6, 'Switches', 'hardware-networking-switches'),
    (14, 6, 'Firewalls', 'hardware-networking-firewalls'),
    (15, 7, 'Dashboards', 'software-analytics-dashboards'),
    (16, 8, 'Endpoint Security', 'software-security-endpoint'),
    (17, 9, 'Collaboration', 'software-productivity-collaboration'),
    (18, 10, 'Implementation', 'services-consulting-implementation');

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 80
)
INSERT INTO suppliers (
    id,
    organization_id,
    supplier_code,
    name,
    email,
    phone,
    lead_time_days,
    rating,
    is_preferred,
    created_at
)
SELECT
    n,
    ((n - 1) % 12) + 1,
    printf('SUP-%04d', n),
    printf('Supplier %03d', n),
    printf('supplier%03d@example.test', n),
    printf('555-02%04d', n),
    3 + (n % 18),
    round(3.2 + ((n % 18) / 10.0), 2),
    CASE WHEN n % 5 = 0 THEN 1 ELSE 0 END,
    datetime('2021-01-01T09:00:00Z', printf('+%d days', n * 3))
FROM seq;

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 2500
)
INSERT INTO products (
    id,
    category_id,
    supplier_id,
    sku,
    name,
    description,
    unit_price,
    unit_cost,
    status,
    weight_kg,
    attributes_json,
    created_at
)
SELECT
    n,
    ((n - 1) % 15) + 4,
    ((n - 1) % 80) + 1,
    printf('SKU-%05d', n),
    printf('Product %05d', n),
    printf('Product %05d long description with dimensions and lifecycle metadata.', n),
    round(15 + ((n % 200) * 2.35), 2),
    round(8 + ((n % 150) * 1.41), 2),
    CASE n % 6
        WHEN 0 THEN 'draft'
        WHEN 1 THEN 'active'
        WHEN 2 THEN 'active'
        WHEN 3 THEN 'active'
        WHEN 4 THEN 'backorder'
        ELSE 'discontinued'
    END,
    round(0.2 + ((n % 35) / 3.0), 3),
    printf('{"color":"c%02d","size":"s%02d","fragile":%s}', n % 12, n % 7, CASE WHEN n % 9 = 0 THEN 'true' ELSE 'false' END),
    datetime('2021-01-01T10:00:00Z', printf('+%d hours', n))
FROM seq;

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 24
)
INSERT INTO warehouses (
    id,
    organization_id,
    code,
    name,
    city,
    region,
    capacity_units,
    is_active
)
SELECT
    n,
    ((n - 1) % 12) + 1,
    printf('WH-%03d', n),
    printf('Warehouse %02d', n),
    CASE n % 6
        WHEN 0 THEN 'Austin'
        WHEN 1 THEN 'Denver'
        WHEN 2 THEN 'Chicago'
        WHEN 3 THEN 'Seattle'
        WHEN 4 THEN 'Boston'
        ELSE 'Atlanta'
    END,
    CASE WHEN n % 2 = 0 THEN 'east' ELSE 'west' END,
    20000 + (n * 1750),
    CASE WHEN n = 24 THEN 0 ELSE 1 END
FROM seq;

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 1200
)
INSERT INTO customers (
    id,
    organization_id,
    customer_code,
    company_name,
    contact_name,
    email,
    phone,
    segment,
    credit_limit,
    balance,
    is_active,
    notes,
    created_at
)
SELECT
    n,
    ((n - 1) % 12) + 1,
    printf('CUST-%05d', n),
    printf('Customer Company %04d', n),
    printf('Contact %04d', n),
    CASE WHEN n % 19 = 0 THEN NULL ELSE printf('customer%04d@example.test', n) END,
    CASE WHEN n % 23 = 0 THEN NULL ELSE printf('555-03%04d', n) END,
    CASE n % 4
        WHEN 0 THEN 'enterprise'
        WHEN 1 THEN 'mid_market'
        WHEN 2 THEN 'smb'
        ELSE 'public_sector'
    END,
    2500 + (n % 20) * 1250,
    round((n % 17) * 87.35, 2),
    CASE WHEN n % 41 = 0 THEN 0 ELSE 1 END,
    CASE WHEN n % 13 = 0 THEN 'Escalated account with quarterly review cadence.' ELSE NULL END,
    datetime('2021-01-01T07:00:00Z', printf('+%d days', n % 900))
FROM seq;

INSERT INTO addresses (
    id,
    customer_id,
    address_type,
    line1,
    line2,
    city,
    state_code,
    postal_code,
    country_code,
    is_default,
    created_at
)
SELECT
    (id * 2) - 1,
    id,
    'billing',
    printf('%d Market Street', id),
    CASE WHEN id % 6 = 0 THEN 'Suite 500' ELSE NULL END,
    CASE id % 5
        WHEN 0 THEN 'New York'
        WHEN 1 THEN 'San Francisco'
        WHEN 2 THEN 'Dallas'
        WHEN 3 THEN 'Portland'
        ELSE 'Miami'
    END,
    CASE id % 5
        WHEN 0 THEN 'NY'
        WHEN 1 THEN 'CA'
        WHEN 2 THEN 'TX'
        WHEN 3 THEN 'OR'
        ELSE 'FL'
    END,
    printf('%05d', 10000 + id),
    'US',
    1,
    datetime(created_at, '+1 day')
FROM customers;

INSERT INTO addresses (
    id,
    customer_id,
    address_type,
    line1,
    line2,
    city,
    state_code,
    postal_code,
    country_code,
    is_default,
    created_at
)
SELECT
    id * 2,
    id,
    'shipping',
    printf('%d Distribution Way', id),
    CASE WHEN id % 9 = 0 THEN 'Dock 3' ELSE NULL END,
    CASE id % 5
        WHEN 0 THEN 'Buffalo'
        WHEN 1 THEN 'Oakland'
        WHEN 2 THEN 'Houston'
        WHEN 3 THEN 'Salem'
        ELSE 'Tampa'
    END,
    CASE id % 5
        WHEN 0 THEN 'NY'
        WHEN 1 THEN 'CA'
        WHEN 2 THEN 'TX'
        WHEN 3 THEN 'OR'
        ELSE 'FL'
    END,
    printf('%05d', 20000 + id),
    'US',
    1,
    datetime(created_at, '+2 days')
FROM customers
WHERE id <= 900;

INSERT INTO inventory (
    product_id,
    warehouse_id,
    quantity_on_hand,
    reserved_quantity,
    reorder_point,
    last_counted_at
)
SELECT
    p.id,
    w.id,
    20 + ((p.id * w.id) % 300),
    (p.id + w.id) % 17,
    15 + ((p.id + w.id) % 45),
    datetime('2024-12-31T00:00:00Z', printf('-%d days', (p.id + w.id) % 180))
FROM products AS p
JOIN warehouses AS w
    ON ((p.id + (w.id * 3)) % 7) < 2;

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 15000
)
INSERT INTO orders (
    id,
    customer_id,
    sales_rep_user_id,
    order_number,
    status,
    order_date,
    required_date,
    shipped_date,
    currency_code,
    subtotal,
    tax_amount,
    shipping_amount,
    discount_amount,
    total_amount,
    source,
    notes,
    updated_at
)
SELECT
    n,
    ((n - 1) % 1200) + 1,
    ((n - 1) % 600) + 1,
    printf('SO-%06d', n),
    CASE n % 7
        WHEN 0 THEN 'draft'
        WHEN 1 THEN 'submitted'
        WHEN 2 THEN 'approved'
        WHEN 3 THEN 'picking'
        WHEN 4 THEN 'shipped'
        WHEN 5 THEN 'delivered'
        ELSE 'cancelled'
    END,
    datetime('2023-01-01T08:00:00Z', printf('+%d hours', n * 3)),
    datetime('2023-01-01T08:00:00Z', printf('+%d hours', (n * 3) + 96)),
    CASE WHEN n % 7 IN (4, 5) THEN datetime('2023-01-01T08:00:00Z', printf('+%d hours', (n * 3) + 72)) ELSE NULL END,
    'USD',
    0,
    0,
    0,
    0,
    0,
    CASE n % 4
        WHEN 0 THEN 'web'
        WHEN 1 THEN 'sales'
        WHEN 2 THEN 'partner'
        ELSE 'api'
    END,
    CASE WHEN n % 29 = 0 THEN 'Rush handling requested.' ELSE NULL END,
    datetime('2023-01-01T08:00:00Z', printf('+%d hours', n * 3))
FROM seq;

WITH line_numbers(line_number) AS (
    VALUES (1), (2), (3)
)
INSERT INTO order_items (
    order_id,
    product_id,
    line_number,
    quantity,
    unit_price,
    discount_percent,
    tax_percent,
    line_total
)
SELECT
    o.id,
    (((o.id * 13) + (ln.line_number * 17)) % 2500) + 1,
    ln.line_number,
    1 + ((o.id + ln.line_number) % 7),
    round(20 + (((o.id * 13) + (ln.line_number * 17)) % 2500) % 200 * 2.35, 2),
    CASE WHEN (o.id + ln.line_number) % 5 = 0 THEN 0.10 ELSE 0.00 END,
    CASE WHEN (o.id + ln.line_number) % 4 = 0 THEN 0.0825 ELSE 0.06 END,
    round(
        (1 + ((o.id + ln.line_number) % 7)) *
        (20 + ((((o.id * 13) + (ln.line_number * 17)) % 2500) % 200 * 2.35)) *
        (1 - CASE WHEN (o.id + ln.line_number) % 5 = 0 THEN 0.10 ELSE 0.00 END) *
        (1 + CASE WHEN (o.id + ln.line_number) % 4 = 0 THEN 0.0825 ELSE 0.06 END),
        2
    )
FROM orders AS o
CROSS JOIN line_numbers AS ln;

UPDATE orders
SET
    subtotal = (
        SELECT round(COALESCE(SUM(quantity * unit_price), 0), 2)
        FROM order_items
        WHERE order_id = orders.id
    ),
    tax_amount = (
        SELECT round(COALESCE(SUM((quantity * unit_price * (1 - discount_percent)) * tax_percent), 0), 2)
        FROM order_items
        WHERE order_id = orders.id
    ),
    discount_amount = (
        SELECT round(COALESCE(SUM(quantity * unit_price * discount_percent), 0), 2)
        FROM order_items
        WHERE order_id = orders.id
    ),
    shipping_amount = round(5 + (orders.id % 6) * 3.5, 2),
    total_amount = (
        SELECT round(
            COALESCE(SUM((quantity * unit_price * (1 - discount_percent)) * (1 + tax_percent)), 0),
            2
        )
        FROM order_items
        WHERE order_id = orders.id
    ) + round(5 + (orders.id % 6) * 3.5, 2);

INSERT INTO payments (
    order_id,
    payment_reference,
    paid_at,
    amount,
    method,
    status,
    processor_response_json
)
SELECT
    id,
    printf('PAY-%06d', id),
    datetime(order_date, '+2 days'),
    round(total_amount - CASE WHEN id % 11 = 0 THEN 25 ELSE 0 END, 2),
    CASE id % 4
        WHEN 0 THEN 'card'
        WHEN 1 THEN 'ach'
        WHEN 2 THEN 'wire'
        ELSE 'check'
    END,
    CASE
        WHEN status IN ('shipped', 'delivered') THEN 'captured'
        WHEN status IN ('approved', 'picking') THEN 'authorized'
        WHEN status = 'cancelled' THEN 'voided'
        ELSE 'pending'
    END,
    printf('{"gateway":"g%02d","avs":"%s","retries":%d}', id % 5, CASE WHEN id % 9 = 0 THEN 'retry' ELSE 'match' END, id % 3)
FROM orders
WHERE status <> 'draft';

INSERT INTO shipments (
    order_id,
    warehouse_id,
    tracking_number,
    carrier,
    shipped_at,
    delivered_at,
    status,
    shipping_cost
)
SELECT
    id,
    ((id - 1) % 24) + 1,
    printf('TRK-%08d', id),
    CASE id % 4
        WHEN 0 THEN 'UPS'
        WHEN 1 THEN 'FedEx'
        WHEN 2 THEN 'DHL'
        ELSE 'USPS'
    END,
    COALESCE(shipped_date, datetime(order_date, '+4 days')),
    CASE WHEN status = 'delivered' THEN datetime(order_date, '+7 days') ELSE NULL END,
    CASE
        WHEN status = 'delivered' THEN 'delivered'
        WHEN status = 'shipped' THEN 'in_transit'
        ELSE 'label_created'
    END,
    shipping_amount
FROM orders
WHERE status IN ('approved', 'picking', 'shipped', 'delivered');

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 3000
)
INSERT INTO support_tickets (
    id,
    customer_id,
    owner_user_id,
    priority,
    status,
    subject,
    description,
    opened_at,
    closed_at,
    tags_json
)
SELECT
    n,
    ((n - 1) % 1200) + 1,
    ((n - 1) % 600) + 1,
    CASE n % 4
        WHEN 0 THEN 'low'
        WHEN 1 THEN 'medium'
        WHEN 2 THEN 'high'
        ELSE 'urgent'
    END,
    CASE n % 5
        WHEN 0 THEN 'new'
        WHEN 1 THEN 'triaged'
        WHEN 2 THEN 'waiting_customer'
        WHEN 3 THEN 'resolved'
        ELSE 'closed'
    END,
    printf('Ticket subject %05d', n),
    printf('Detailed ticket description for case %05d with reproducible steps.', n),
    datetime('2024-01-01T09:00:00Z', printf('+%d hours', n * 2)),
    CASE WHEN n % 5 IN (3, 4) THEN datetime('2024-01-01T09:00:00Z', printf('+%d hours', (n * 2) + 36)) ELSE NULL END,
    printf('["tag%d","tag%d"]', n % 8, (n + 3) % 8)
FROM seq;

WITH comment_slots(slot) AS (
    VALUES (1), (2), (3)
)
INSERT INTO ticket_comments (
    ticket_id,
    author_user_id,
    body,
    created_at,
    is_internal
)
SELECT
    t.id,
    ((t.id + slot) % 600) + 1,
    printf('Comment %d for ticket %05d with operational detail.', slot, t.id),
    datetime(t.opened_at, printf('+%d hours', slot * 6)),
    CASE WHEN slot = 3 AND t.id % 4 = 0 THEN 1 ELSE 0 END
FROM support_tickets AS t
JOIN comment_slots
    ON 1 = 1;

WITH flags(flag_key, base_enabled) AS (
    VALUES
        ('new_dashboard', 1),
        ('bulk_export', 1),
        ('advanced_pricing', 0),
        ('support_console', 1),
        ('auto_reorder', 0)
)
INSERT INTO feature_flags (
    organization_id,
    flag_key,
    enabled,
    rollout_percent,
    rules_json
)
SELECT
    o.id,
    f.flag_key,
    CASE WHEN (o.id + f.base_enabled) % 3 = 0 THEN 0 ELSE f.base_enabled END,
    ((o.id * 7) + length(f.flag_key)) % 101,
    printf('{"allowlist":["org-%02d"],"environment":"prod","seed":%d}', o.id, length(f.flag_key))
FROM organizations AS o
CROSS JOIN flags AS f;

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 2000
)
INSERT INTO documents (
    id,
    customer_id,
    file_name,
    mime_type,
    size_bytes,
    checksum,
    content_blob,
    created_at
)
SELECT
    n,
    CASE WHEN n % 10 = 0 THEN NULL ELSE ((n - 1) % 1200) + 1 END,
    printf('document_%05d.%s', n, CASE WHEN n % 3 = 0 THEN 'pdf' WHEN n % 3 = 1 THEN 'csv' ELSE 'txt' END),
    CASE WHEN n % 3 = 0 THEN 'application/pdf' WHEN n % 3 = 1 THEN 'text/csv' ELSE 'text/plain' END,
    256 + (n % 4096),
    lower(hex(randomblob(16))),
    randomblob(128),
    datetime('2024-06-01T10:00:00Z', printf('+%d minutes', n))
FROM seq;

WITH RECURSIVE days(d) AS (
    SELECT 0
    UNION ALL
    SELECT d + 1 FROM days WHERE d < 364
)
INSERT INTO metrics_daily (
    organization_id,
    metric_date,
    active_users,
    new_customers,
    order_count,
    gross_revenue,
    return_rate
)
SELECT
    o.id,
    date('2024-01-01', printf('+%d days', d)),
    18 + ((o.id * 3 + d) % 240),
    (o.id + d) % 15,
    5 + ((o.id * 5 + d) % 80),
    round(1000 + ((o.id * 175) + (d * 42)) % 35000 + ((d % 7) * 125.5), 2),
    round(((o.id + d) % 9) / 100.0, 4)
FROM organizations AS o
CROSS JOIN days;

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 20000
)
INSERT INTO events (
    id,
    stream_name,
    event_type,
    aggregate_id,
    sequence_no,
    published_at,
    payload_json,
    correlation_id
)
SELECT
    n,
    CASE n % 4
        WHEN 0 THEN 'orders'
        WHEN 1 THEN 'customers'
        WHEN 2 THEN 'inventory'
        ELSE 'support'
    END,
    CASE n % 5
        WHEN 0 THEN 'created'
        WHEN 1 THEN 'updated'
        WHEN 2 THEN 'synced'
        WHEN 3 THEN 'dispatched'
        ELSE 'closed'
    END,
    printf('agg-%05d', ((n - 1) % 2500) + 1),
    ((n - 1) / 2500) + 1,
    datetime('2024-01-01T00:00:00Z', printf('+%d minutes', n)),
    printf('{"entityId":%d,"version":%d,"source":"seed"}', ((n - 1) % 2500) + 1, ((n - 1) / 2500) + 1),
    printf('corr-%08d', (n * 17) % 100000000)
FROM seq;

WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 50000
)
INSERT INTO audit_log (
    id,
    actor_user_id,
    entity_type,
    entity_id,
    action,
    changed_at,
    ip_address,
    metadata_json,
    payload_blob
)
SELECT
    n,
    CASE WHEN n % 9 = 0 THEN NULL ELSE ((n - 1) % 600) + 1 END,
    CASE n % 6
        WHEN 0 THEN 'order'
        WHEN 1 THEN 'customer'
        WHEN 2 THEN 'product'
        WHEN 3 THEN 'ticket'
        WHEN 4 THEN 'shipment'
        ELSE 'payment'
    END,
    ((n - 1) % 15000) + 1,
    CASE n % 5
        WHEN 0 THEN 'insert'
        WHEN 1 THEN 'update'
        WHEN 2 THEN 'status_change'
        WHEN 3 THEN 'sync'
        ELSE 'comment'
    END,
    datetime('2024-01-01T00:00:00Z', printf('+%d minutes', n)),
    printf('10.%d.%d.%d', n % 255, (n * 3) % 255, (n * 7) % 255),
    printf('{"batch":%d,"source":"seed","shard":"%s"}', n % 400, CASE WHEN n % 2 = 0 THEN 'east' ELSE 'west' END),
    CASE WHEN n % 8 = 0 THEN randomblob(24) ELSE NULL END
FROM seq;

CREATE INDEX idx_users_org_active ON users(organization_id, is_active);
CREATE INDEX idx_customers_org_segment ON customers(organization_id, segment);
CREATE INDEX idx_customers_email_partial ON customers(email) WHERE email IS NOT NULL;
CREATE INDEX idx_addresses_customer_type ON addresses(customer_id, address_type);
CREATE INDEX idx_products_category_status ON products(category_id, status);
CREATE INDEX idx_products_supplier ON products(supplier_id);
CREATE INDEX idx_inventory_warehouse_reorder ON inventory(warehouse_id, reorder_point, quantity_on_hand);
CREATE INDEX idx_orders_customer_date ON orders(customer_id, order_date DESC);
CREATE INDEX idx_orders_open_status ON orders(status, required_date) WHERE status NOT IN ('delivered', 'cancelled');
CREATE INDEX idx_order_items_product ON order_items(product_id);
CREATE INDEX idx_payments_order_status ON payments(order_id, status);
CREATE INDEX idx_shipments_status_shipped_at ON shipments(status, shipped_at DESC);
CREATE INDEX idx_support_tickets_customer_status ON support_tickets(customer_id, status);
CREATE INDEX idx_ticket_comments_ticket_created ON ticket_comments(ticket_id, created_at);
CREATE INDEX idx_audit_log_entity_changed ON audit_log(entity_type, entity_id, changed_at DESC);
CREATE INDEX idx_events_stream_published ON events(stream_name, published_at DESC);
CREATE INDEX idx_documents_customer_created ON documents(customer_id, created_at DESC);

CREATE VIEW v_order_summary AS
SELECT
    o.id,
    o.order_number,
    o.status,
    o.order_date,
    o.total_amount,
    c.customer_code,
    c.company_name,
    u.full_name AS sales_rep_name
FROM orders AS o
JOIN customers AS c ON c.id = o.customer_id
LEFT JOIN users AS u ON u.id = o.sales_rep_user_id;

CREATE VIEW v_customer_lifetime_value AS
SELECT
    c.id AS customer_id,
    c.customer_code,
    c.company_name,
    COUNT(o.id) AS order_count,
    round(COALESCE(SUM(o.total_amount), 0), 2) AS lifetime_value,
    round(COALESCE(AVG(o.total_amount), 0), 2) AS average_order_value
FROM customers AS c
LEFT JOIN orders AS o ON o.customer_id = c.id
GROUP BY c.id, c.customer_code, c.company_name;

CREATE VIEW v_inventory_alerts AS
SELECT
    i.id,
    p.sku,
    p.name AS product_name,
    w.code AS warehouse_code,
    i.quantity_on_hand,
    i.reserved_quantity,
    i.reorder_point
FROM inventory AS i
JOIN products AS p ON p.id = i.product_id
JOIN warehouses AS w ON w.id = i.warehouse_id
WHERE i.quantity_on_hand - i.reserved_quantity <= i.reorder_point;

CREATE VIEW v_top_selling_products AS
SELECT
    p.id AS product_id,
    p.sku,
    p.name,
    SUM(oi.quantity) AS units_sold,
    round(SUM(oi.line_total), 2) AS revenue
FROM order_items AS oi
JOIN products AS p ON p.id = oi.product_id
GROUP BY p.id, p.sku, p.name
ORDER BY revenue DESC, units_sold DESC;

CREATE VIEW v_support_ticket_backlog AS
SELECT
    customer_id,
    status,
    priority,
    COUNT(*) AS ticket_count
FROM support_tickets
WHERE status NOT IN ('resolved', 'closed')
GROUP BY customer_id, status, priority;

CREATE TRIGGER trg_orders_touch_updated_at
AFTER UPDATE ON orders
FOR EACH ROW
WHEN NEW.updated_at = OLD.updated_at
BEGIN
    UPDATE orders
    SET updated_at = datetime('now')
    WHERE id = NEW.id;
END;

CREATE TRIGGER trg_order_items_after_insert
AFTER INSERT ON order_items
FOR EACH ROW
BEGIN
    UPDATE orders
    SET
        subtotal = (
            SELECT round(COALESCE(SUM(quantity * unit_price), 0), 2)
            FROM order_items
            WHERE order_id = NEW.order_id
        ),
        tax_amount = (
            SELECT round(COALESCE(SUM((quantity * unit_price * (1 - discount_percent)) * tax_percent), 0), 2)
            FROM order_items
            WHERE order_id = NEW.order_id
        ),
        discount_amount = (
            SELECT round(COALESCE(SUM(quantity * unit_price * discount_percent), 0), 2)
            FROM order_items
            WHERE order_id = NEW.order_id
        ),
        total_amount = (
            SELECT round(COALESCE(SUM(line_total), 0), 2)
            FROM order_items
            WHERE order_id = NEW.order_id
        ) + shipping_amount
    WHERE id = NEW.order_id;
END;

CREATE TRIGGER trg_order_items_after_update
AFTER UPDATE ON order_items
FOR EACH ROW
BEGIN
    UPDATE orders
    SET
        subtotal = (
            SELECT round(COALESCE(SUM(quantity * unit_price), 0), 2)
            FROM order_items
            WHERE order_id = NEW.order_id
        ),
        tax_amount = (
            SELECT round(COALESCE(SUM((quantity * unit_price * (1 - discount_percent)) * tax_percent), 0), 2)
            FROM order_items
            WHERE order_id = NEW.order_id
        ),
        discount_amount = (
            SELECT round(COALESCE(SUM(quantity * unit_price * discount_percent), 0), 2)
            FROM order_items
            WHERE order_id = NEW.order_id
        ),
        total_amount = (
            SELECT round(COALESCE(SUM(line_total), 0), 2)
            FROM order_items
            WHERE order_id = NEW.order_id
        ) + shipping_amount
    WHERE id = NEW.order_id;
END;

CREATE TRIGGER trg_order_items_after_delete
AFTER DELETE ON order_items
FOR EACH ROW
BEGIN
    UPDATE orders
    SET
        subtotal = (
            SELECT round(COALESCE(SUM(quantity * unit_price), 0), 2)
            FROM order_items
            WHERE order_id = OLD.order_id
        ),
        tax_amount = (
            SELECT round(COALESCE(SUM((quantity * unit_price * (1 - discount_percent)) * tax_percent), 0), 2)
            FROM order_items
            WHERE order_id = OLD.order_id
        ),
        discount_amount = (
            SELECT round(COALESCE(SUM(quantity * unit_price * discount_percent), 0), 2)
            FROM order_items
            WHERE order_id = OLD.order_id
        ),
        total_amount = (
            SELECT round(COALESCE(SUM(line_total), 0), 2)
            FROM order_items
            WHERE order_id = OLD.order_id
        ) + shipping_amount
    WHERE id = OLD.order_id;
END;

CREATE TRIGGER trg_payments_audit
AFTER INSERT ON payments
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (
        actor_user_id,
        entity_type,
        entity_id,
        action,
        changed_at,
        ip_address,
        metadata_json,
        payload_blob
    )
    VALUES (
        NULL,
        'payment',
        NEW.id,
        'insert',
        NEW.paid_at,
        '127.0.0.1',
        '{"source":"trigger","table":"payments"}',
        NULL
    );
END;

COMMIT;

ANALYZE;
