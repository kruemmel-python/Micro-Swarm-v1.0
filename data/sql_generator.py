import streamlit as st
from faker import Faker
import io

# Konfiguration der Seite
st.set_page_config(page_title="SQL Data Generator", page_icon="📊")

def generate_sql_script(table_name, rows, selected_fields, locale):
    fake = Faker(locale)
    
    # Mapping von Anzeigenamen zu Faker-Funktionen und SQL-Typen
    field_map = {
        "Vorname": (fake.first_name, "VARCHAR(100)"),
        "Nachname": (fake.last_name, "VARCHAR(100)"),
        "E-Mail": (fake.email, "VARCHAR(255)"),
        "Telefonnummer": (fake.phone_number, "VARCHAR(50)"),
        "Straße": (fake.street_address, "VARCHAR(255)"),
        "Stadt": (fake.city, "VARCHAR(100)"),
        "PLZ": (fake.postcode, "VARCHAR(20)"),
        "Geburtsdatum": (lambda: fake.date_of_birth(minimum_age=18, maximum_age=90).strftime('%Y-%m-%d'), "DATE"),
        "Firmenname": (fake.company, "VARCHAR(255)"),
        "Job": (fake.job, "VARCHAR(100)"),
        "IBAN": (fake.iban, "VARCHAR(34)"),
        "Text (kurz)": (lambda: fake.sentence(nb_words=6), "TEXT"),
    }

    sql_output = io.StringIO()
    
    # CREATE TABLE Statement
    sql_output.write(f"CREATE TABLE {table_name} (\n")
    sql_output.write("    id SERIAL PRIMARY KEY,\n")
    
    cols_sql = []
    for field in selected_fields:
        _, dtype = field_map[field]
        column_name = field.lower().replace(" ", "_").replace("-", "_")
        cols_sql.append(f"    {column_name} {dtype}")
    
    sql_output.write(",\n".join(cols_sql))
    sql_output.write("\n);\n\n")

    # INSERT INTO Statements
    column_names_str = ", ".join([f.lower().replace(" ", "_").replace("-", "_") for f in selected_fields])
    
    for _ in range(rows):
        values = []
        for field in selected_fields:
            func, _ = field_map[field]
            val = func()
            # Escaping für SQL (einfache Anführungszeichen)
            val = str(val).replace("'", "''")
            values.append(f"'{val}'")
        
        values_str = ", ".join(values)
        sql_output.write(f"INSERT INTO {table_name} ({column_names_str}) VALUES ({values_str});\n")

    return sql_output.getvalue()

# Streamlit UI
st.title("🚀 SQL Testdaten Generator")
st.write("Erstelle realistische Testdaten mit echten Namen für deine Datenbank.")

# Einstellungen in der Seitenleiste
st.sidebar.header("Einstellungen")
table_name = st.sidebar.text_input("Tabellenname", "users")
num_rows = st.sidebar.number_input("Anzahl Datensätze", min_value=1, max_value=10000, value=100)
locale = st.sidebar.selectbox("Sprache der Daten", ["de_DE", "en_US", "fr_FR", "es_ES"], index=0)

# Auswahl der Felder
available_fields = [
    "Vorname", "Nachname", "E-Mail", "Telefonnummer", 
    "Straße", "Stadt", "PLZ", "Geburtsdatum", 
    "Firmenname", "Job", "IBAN", "Text (kurz)"
]

selected_fields = st.multiselect(
    "Welche Felder sollen generiert werden?",
    available_fields,
    default=["Vorname", "Nachname", "E-Mail", "Stadt"]
)

if st.button("SQL Script generieren"):
    if not selected_fields:
        st.error("Bitte wähle mindestens ein Feld aus!")
    else:
        with st.spinner('Generiere Daten...'):
            sql_data = generate_sql_script(table_name, num_rows, selected_fields, locale)
            
            st.success(f"{num_rows} Datensätze erfolgreich generiert!")
            
            # Vorschau
            st.text_area("Vorschau (erste 500 Zeichen)", sql_data[:500] + "...", height=200)
            
            # Download Button
            st.download_button(
                label="📥 SQL Datei herunterladen",
                data=sql_data,
                file_name=f"{table_name}_data.sql",
                mime="text/sql"
            )

st.info("Hinweis: Die Namen und Daten sind zufällig generiert ('Fake'), sehen aber durch die Faker-Library wie echte Daten aus.")