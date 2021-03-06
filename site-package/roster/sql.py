import mysql.connector

SQL_CREATE_TABLE = \
"""
CREATE TABLE `{}` (
    `date` date NOT NULL UNIQUE,
    `chair` char(64) NOT NULL DEFAULT '',
    `minute` char(64) NOT NULL DEFAULT '',
    PRIMARY KEY (`date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8
"""


def create_database(cursor, database_name):
    try:
        cursor.execute("CREATE DATABASE `{}` DEFAULT CHARACTER SET 'utf8'".format(database_name))
    except mysql.connector.Error as e:
        print("Error [{}]: failed to create database [{}]".format(e, database_name))
        raise Exception("MySQL")


def create_table(cursor, table_name):
    try:
        cursor.execute(SQL_CREATE_TABLE.format(table_name))
    except mysql.connector.Error as e:
        print("Error [{}]: failed to create table [{}]".format(e, table_name))
        raise Exception("MySQL")


def establish_connection(config):
    # Connection to server
    connection = mysql.connector.connect(**config)

    return connection


def close_connection(connection):
    connection.close()


def connect_database(connection, database_name):
    # Connect to database, or create a new one
    try:
        connection.database = database_name
    except mysql.connector.Error as e:
        if e.errno == 1049:
            # Get cursor
            cursor = connection.cursor()

            print("Creating database [{}]".format(database_name))
            create_database(cursor, database_name)

            # Close cursor
            cursor.close()
            connection.database = database_name
        else:
            print("Error [{}]: connect database".format(e))
            raise Exception("MySQL")


def entry_exists(connection, table_name, condition):
    cursor = connection.cursor()

    sql = "SELECT COUNT(*) FROM `{}` WHERE {}".format(table_name, condition)
    # print(sql)
    try:
        cursor.execute(sql)
        for result in cursor:
            if result[0] == 0:
                cursor.close()
                return False
            else:
                cursor.close()
                return True
    except mysql.connector.Error as e:
        if e.errno == 1146:  # Table doesn't exist
            print("Creating table [{}]".format(table_name))
            create_table(cursor, table_name)
            cursor.close()
            return False
        else:
            print("Error [{}]: entry exists".format(e))
            print(sql)
            cursor.close()
            raise Exception("MySQL")

def fetch_entry(connection, table_name, condition):
    cursor = connection.cursor()

    sql = "SELECT `chair`, `minute` from `{}` WHERE {}".format(table_name, condition)

    try:
        cursor.execute(sql)
        for result in cursor:
            return result[0], result[1]
    except mysql.connector.Error as e:
        if e.errno == 1146:  # Table doesn't exist
            print("Creating table [{}]".format(table_name))
            create_table(cursor, table_name)
            cursor.close()
            return False
        else:
            print("Error [{}]: entry exists".format(e))
            print(sql)
            cursor.close()
            raise Exception("MySQL")


def insert_entry(connection, table_name, value):
    cursor = connection.cursor()

    sql = "INSERT INTO `{}` {}".format(table_name, value)
    # print(sql)
    try:
        cursor.execute(sql)
        cursor.close()
    except mysql.connector.Error as e:
        if e.errno == 1146:  # Table doesn't exist
            print("Creating table [{}]".format(table_name))
            create_table(cursor, table_name)

            # Try to execute again
            cursor.execute(sql)

            cursor.close()
        else:
            print("Error [{}]: insert entry".format(e))
            print(sql)
            cursor.close()
            raise Exception("MySQL")


def main():
    SQL_CONFIG = {
        "host": "192.168.2.5",
        "user": "eamars",
        "password": "931105",
        "autocommit": True
    }

    connection = establish_connection(SQL_CONFIG)
    connect_database(connection, "test")

    print(entry_exists(connection, "roster", "chair=`Ran Bao`"))
    close_connection(connection)


if __name__ == "__main__":
    main()
