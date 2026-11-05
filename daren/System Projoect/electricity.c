#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define DATA_FILE "electricity_records.txt"
#define MAX_RECORDS 500
#define NAME_LENGTH 100
#define INPUT_LENGTH 200
#define MAX_ATTEMPTS 3

typedef struct {
    int id;
    char customerName[NAME_LENGTH];
    double previousReading;
    double currentReading;
    double ratePerKwh;
} ElectricityRecord;

ElectricityRecord records[MAX_RECORDS];
int recordCount = 0;
int nextId = 1;

void trimWhitespace(char *text) {
    size_t start = 0;
    size_t length;

    while (isspace((unsigned char)text[start])) {
        start++;
    }

    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }

    length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[--length] = '\0';
    }
}

int readLine(const char *prompt, char *buffer, size_t size) {
    int ch;
    size_t length;

    printf("%s", prompt);

    if (fgets(buffer, (int)size, stdin) == NULL) {
        return 0;
    }

    length = strlen(buffer);

    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else {
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
    }

    trimWhitespace(buffer);
    return 1;
}

int isValidName(const char *name) {
    int hasLetter = 0;

    if (name[0] == '\0') {
        return 0;
    }

    for (size_t i = 0; name[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)name[i];

        if (isalpha(ch)) {
            hasLetter = 1;
        } else if (ch != ' ') {
            return 0;
        }
    }

    return hasLetter;
}

int isValidIntegerText(const char *text) {
    if (text[0] == '\0') {
        return 0;
    }

    for (size_t i = 0; text[i] != '\0'; i++) {
        if (!isdigit((unsigned char)text[i])) {
            return 0;
        }
    }

    return 1;
}

int isValidDecimalText(const char *text) {
    int digitCount = 0;
    int decimalCount = 0;

    if (text[0] == '\0') {
        return 0;
    }

    for (size_t i = 0; text[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)text[i];

        if (isdigit(ch)) {
            digitCount++;
        } else if (ch == '.' && decimalCount == 0) {
            decimalCount++;
        } else {
            return 0;
        }
    }

    return digitCount > 0;
}

int inputName(const char *prompt, char *name, size_t size) {
    char input[INPUT_LENGTH];

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        if (!readLine(prompt, input, sizeof(input))) {
            return 0;
        }

        if (isValidName(input) && strlen(input) < size) {
            strcpy(name, input);
            return 1;
        }

        printf("Wrong name. Please input a valid name (letters only).\n");
        if (attempt < MAX_ATTEMPTS) {
            printf("Attempts remaining: %d\n", MAX_ATTEMPTS - attempt);
        }
    }

    printf("Maximum attempts reached. Returning to the main menu.\n");
    return 0;
}

int inputInteger(const char *prompt, int *value) {
    char input[INPUT_LENGTH];
    char *end;
    long parsed;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        if (!readLine(prompt, input, sizeof(input))) {
            return 0;
        }

        if (isValidIntegerText(input)) {
            errno = 0;
            parsed = strtol(input, &end, 10);

            if (errno != ERANGE && *end == '\0' &&
                parsed >= 0 && parsed <= 2147483647L) {
                *value = (int)parsed;
                return 1;
            }
        }

        printf("Wrong number. Please input a valid number (digits only).\n");
        if (attempt < MAX_ATTEMPTS) {
            printf("Attempts remaining: %d\n", MAX_ATTEMPTS - attempt);
        }
    }

    printf("Maximum attempts reached. Returning to the main menu.\n");
    return 0;
}

int inputDecimal(const char *prompt, double *value) {
    char input[INPUT_LENGTH];
    char *end;
    double parsed;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        if (!readLine(prompt, input, sizeof(input))) {
            return 0;
        }

        if (isValidDecimalText(input)) {
            errno = 0;
            parsed = strtod(input, &end);

            if (errno != ERANGE && *end == '\0' && parsed >= 0.0) {
                *value = parsed;
                return 1;
            }
        }

        printf("Wrong number. Please input a valid number (digits only; decimal point allowed).\n");
        if (attempt < MAX_ATTEMPTS) {
            printf("Attempts remaining: %d\n", MAX_ATTEMPTS - attempt);
        }
    }

    printf("Maximum attempts reached. Returning to the main menu.\n");
    return 0;
}

int inputYesNo(const char *prompt, int *yes) {
    char input[INPUT_LENGTH];

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        if (!readLine(prompt, input, sizeof(input))) {
            return 0;
        }

        if (strlen(input) == 1 &&
            (toupper((unsigned char)input[0]) == 'Y' ||
             toupper((unsigned char)input[0]) == 'N')) {
            *yes = (toupper((unsigned char)input[0]) == 'Y');
            return 1;
        }

        printf("Invalid input. Please enter Y or N only.\n");
        if (attempt < MAX_ATTEMPTS) {
            printf("Attempts remaining: %d\n", MAX_ATTEMPTS - attempt);
        }
    }

    printf("Maximum attempts reached. Operation cancelled.\n");
    return 0;
}

void saveRecords(void) {
    FILE *file = fopen(DATA_FILE, "w");

    if (file == NULL) {
        printf("Error: Could not save records to %s.\n", DATA_FILE);
        return;
    }

    for (int i = 0; i < recordCount; i++) {
        fprintf(file, "%d|%s|%.2f|%.2f|%.2f\n",
                records[i].id,
                records[i].customerName,
                records[i].previousReading,
                records[i].currentReading,
                records[i].ratePerKwh);
    }

    fclose(file);
}

void loadRecords(void) {
    FILE *file = fopen(DATA_FILE, "r");
    char line[INPUT_LENGTH];

    if (file == NULL) {
        return;
    }

    recordCount = 0;
    nextId = 1;

    while (fgets(line, sizeof(line), file) != NULL &&
           recordCount < MAX_RECORDS) {
        ElectricityRecord record;

        if (sscanf(line, "%d|%99[^|]|%lf|%lf|%lf",
                   &record.id,
                   record.customerName,
                   &record.previousReading,
                   &record.currentReading,
                   &record.ratePerKwh) == 5) {

            if (record.id > 0 &&
                isValidName(record.customerName) &&
                record.previousReading >= 0.0 &&
                record.currentReading >= record.previousReading &&
                record.ratePerKwh >= 0.0) {

                records[recordCount++] = record;

                if (record.id >= nextId) {
                    nextId = record.id + 1;
                }
            }
        }
    }

    fclose(file);
}

int findRecordById(int id) {
    for (int i = 0; i < recordCount; i++) {
        if (records[i].id == id) {
            return i;
        }
    }
    return -1;
}

double calculateConsumption(ElectricityRecord record) {
    return record.currentReading - record.previousReading;
}

double calculateBill(ElectricityRecord record) {
    return calculateConsumption(record) * record.ratePerKwh;
}

void displayRecord(ElectricityRecord record) {
    printf("\n---------------------------------------------\n");
    printf("Record ID        : %d\n", record.id);
    printf("Customer Name    : %s\n", record.customerName);
    printf("Previous Reading : %.2f kWh\n", record.previousReading);
    printf("Current Reading  : %.2f kWh\n", record.currentReading);
    printf("Consumption      : %.2f kWh\n", calculateConsumption(record));
    printf("Rate per kWh     : %.2f\n", record.ratePerKwh);
    printf("Estimated Bill   : %.2f\n", calculateBill(record));
    printf("---------------------------------------------\n");
}

void viewAllRecords(void) {
    printf("\n========== ELECTRICITY CONSUMPTION RECORDS ==========\n");

    if (recordCount == 0) {
        printf("No electricity records found.\n");
        return;
    }

    printf("%-6s %-22s %-13s %-13s %-13s %-13s\n",
           "ID", "CUSTOMER", "PREVIOUS", "CURRENT", "CONSUMPTION", "BILL");
    printf("--------------------------------------------------------------------------------\n");

    for (int i = 0; i < recordCount; i++) {
        printf("%-6d %-22.22s %-13.2f %-13.2f %-13.2f %-13.2f\n",
               records[i].id,
               records[i].customerName,
               records[i].previousReading,
               records[i].currentReading,
               calculateConsumption(records[i]),
               calculateBill(records[i]));
    }
}

void addRecord(void) {
    ElectricityRecord newRecord;

    printf("\n========== ADD NEW ELECTRICITY RECORD ==========\n");

    if (recordCount >= MAX_RECORDS) {
        printf("Record storage is full. Cannot add another record.\n");
        return;
    }

    if (!inputName("Enter customer name: ",
                   newRecord.customerName,
                   sizeof(newRecord.customerName))) {
        return;
    }

    if (!inputDecimal("Enter previous meter reading (kWh): ",
                      &newRecord.previousReading)) {
        return;
    }

    if (!inputDecimal("Enter current meter reading (kWh): ",
                      &newRecord.currentReading)) {
        return;
    }

    if (newRecord.currentReading < newRecord.previousReading) {
        printf("Invalid reading: current reading cannot be lower than previous reading.\n");
        printf("Record was not added. Returning to the main menu.\n");
        return;
    }

    if (!inputDecimal("Enter electricity rate per kWh: ",
                      &newRecord.ratePerKwh)) {
        return;
    }

    newRecord.id = nextId++;
    records[recordCount++] = newRecord;
    saveRecords();

    printf("\nRecord added successfully! Record ID: %d\n", newRecord.id);
}

void calculateConsumptionOption(void) {
    int id;
    int index;

    printf("\n========== CALCULATE ELECTRICITY CONSUMPTION ==========\n");

    if (!inputInteger("Enter record ID: ", &id)) {
        return;
    }

    index = findRecordById(id);

    if (index == -1) {
        printf("Record not found. Returning to the main menu.\n");
        return;
    }

    printf("\nCustomer: %s\n", records[index].customerName);
    printf("Previous reading: %.2f kWh\n", records[index].previousReading);
    printf("Current reading : %.2f kWh\n", records[index].currentReading);
    printf("Consumption     : %.2f kWh\n",
           calculateConsumption(records[index]));
}

void calculateEstimatedBill(void) {
    int id;
    int index;

    printf("\n========== CALCULATE ESTIMATED ELECTRICITY BILL ==========\n");

    if (!inputInteger("Enter record ID: ", &id)) {
        return;
    }

    index = findRecordById(id);

    if (index == -1) {
        printf("Record not found. Returning to the main menu.\n");
        return;
    }

    printf("\nCustomer       : %s\n", records[index].customerName);
    printf("Consumption    : %.2f kWh\n", calculateConsumption(records[index]));
    printf("Rate per kWh   : %.2f\n", records[index].ratePerKwh);
    printf("Estimated bill : %.2f\n", calculateBill(records[index]));
    printf("\nFormula: Consumption x Rate per kWh\n");
}


void viewRecordsOption(void) {
    viewAllRecords();
}

int containsIgnoreCase(const char *text, const char *keyword) {
    size_t textLength = strlen(text);
    size_t keyLength = strlen(keyword);

    if (keyLength == 0) {
        return 0;
    }
    if (keyLength > textLength) {
        return 0;
    }

    for (size_t i = 0; i <= textLength - keyLength; i++) {
        size_t j = 0;

        while (j < keyLength &&
               tolower((unsigned char)text[i + j]) ==
               tolower((unsigned char)keyword[j])) {
            j++;
        }

        if (j == keyLength) {
            return 1;
        }
    }

    return 0;
}

void searchRecord(void) {
    int choice;
    int id;
    int found = 0;
    char keyword[NAME_LENGTH];

    printf("\n========== SEARCH ELECTRICITY RECORD ==========\n");
    printf("1. Search by Record ID\n");
    printf("2. Search by Customer Name\n");

    if (!inputInteger("Enter your choice (1-2): ", &choice)) {
        return;
    }

    if (choice == 1) {
        if (!inputInteger("Enter record ID: ", &id)) {
            return;
        }

        int index = findRecordById(id);
        if (index == -1) {
            printf("Record not found.\n");
        } else {
            displayRecord(records[index]);
        }
    } else if (choice == 2) {
        if (!inputName("Enter customer name or keyword (letters only): ",
                       keyword, sizeof(keyword))) {
            return;
        }

        for (int i = 0; i < recordCount; i++) {
            if (containsIgnoreCase(records[i].customerName, keyword)) {
                displayRecord(records[i]);
                found = 1;
            }
        }

        if (!found) {
            printf("No matching record found.\n");
        }
    } else {
        printf("Invalid choice. Returning to the main menu.\n");
    }
}

void updateRecord(void) {
    int id;
    int index;
    double newPrevious;
    double newCurrent;
    double newRate;

    printf("\n========== UPDATE ELECTRICITY RECORD ==========\n");

    if (!inputInteger("Enter record ID to update: ", &id)) {
        return;
    }

    index = findRecordById(id);

    if (index == -1) {
        printf("Record not found. Returning to the main menu.\n");
        return;
    }

    printf("\nCurrent record details:\n");
    displayRecord(records[index]);

    if (!inputDecimal("Enter new previous reading (kWh): ", &newPrevious)) {
        return;
    }

    if (!inputDecimal("Enter new current reading (kWh): ", &newCurrent)) {
        return;
    }

    if (newCurrent < newPrevious) {
        printf("Invalid reading: current reading cannot be lower than previous reading.\n");
        printf("Update cancelled. Returning to the main menu.\n");
        return;
    }

    if (!inputDecimal("Enter new rate per kWh: ", &newRate)) {
        return;
    }

    records[index].previousReading = newPrevious;
    records[index].currentReading = newCurrent;
    records[index].ratePerKwh = newRate;

    saveRecords();

    printf("\nRecord updated and saved successfully!\n");
    displayRecord(records[index]);
}

void deleteRecord(void) {
    int id;
    int index;
    int confirmed;

    printf("\n========== DELETE ELECTRICITY RECORD ==========\n");

    if (!inputInteger("Enter record ID to delete: ", &id)) {
        return;
    }

    index = findRecordById(id);

    if (index == -1) {
        printf("Record not found. Returning to the main menu.\n");
        return;
    }

    displayRecord(records[index]);

    if (!inputYesNo("Are you sure you want to delete this record? (Y/N): ",
                    &confirmed)) {
        return;
    }

    if (confirmed) {
        for (int i = index; i < recordCount - 1; i++) {
            records[i] = records[i + 1];
        }

        recordCount--;
        saveRecords();
        printf("Record deleted successfully.\n");
    } else {
        printf("Delete cancelled.\n");
    }
}

void viewSummary(void) {
    double totalConsumption = 0.0;
    double totalBill = 0.0;
    double averageConsumption;

    printf("\n========== CONSUMPTION SUMMARY / REPORT ==========\n");

    if (recordCount == 0) {
        printf("No records available. Cannot generate a summary.\n");
        return;
    }

    for (int i = 0; i < recordCount; i++) {
        totalConsumption += calculateConsumption(records[i]);
        totalBill += calculateBill(records[i]);
    }

    averageConsumption = totalConsumption / recordCount;

    printf("Number of records      : %d\n", recordCount);
    printf("Total consumption      : %.2f kWh\n", totalConsumption);
    printf("Total estimated bill   : %.2f\n", totalBill);
    printf("Average consumption    : %.2f kWh\n", averageConsumption);
}

void displayMainMenu(void) {
    printf("\n====================================================\n");
    printf("       ELECTRICAL CONSUMPTION TRACKER SYSTEM\n");
    printf("====================================================\n");
    printf("1. Add New Electricity Record\n");
    printf("2. Calculate Electricity Consumption\n");
    printf("3. Calculate Estimated Electricity Bill\n");
    printf("4. View Electricity Consumption Records\n");
    printf("5. Search Electricity Record\n");
    printf("6. Update Electricity Record\n");
    printf("7. Delete Electricity Record\n");
    printf("8. View Consumption Summary / Report\n");
    printf("9. Exit System\n");
    printf("====================================================\n");
}

int main(void) {
    int choice;
    int invalidMenuAttempts = 0;

    loadRecords();
    printf("Electricity records loaded: %d\n", recordCount);

    while (1) {
        displayMainMenu();

        if (!inputInteger("Enter your choice (1-9): ", &choice)) {
            invalidMenuAttempts++;
            if (invalidMenuAttempts >= MAX_ATTEMPTS) {
                printf("\nYou have used all 3 main-menu attempts.\n");
                break;
            }
            continue;
        }

        if (choice < 1 || choice > 9) {
            invalidMenuAttempts++;
            printf("Wrong number. Please input a valid number from 1 to 9.\n");
            if (invalidMenuAttempts >= MAX_ATTEMPTS) {
                printf("\nYou have used all 3 main-menu attempts.\n");
                break;
            }
            printf("Main-menu attempts remaining: %d\n",
                   MAX_ATTEMPTS - invalidMenuAttempts);
            continue;
        }

        invalidMenuAttempts = 0;

        switch (choice) {
            case 1:
                addRecord();
                break;
            case 2:
                calculateConsumptionOption();
                break;
            case 3:
                calculateEstimatedBill();
                break;
            case 4:
                viewRecordsOption();
                break;
            case 5:
                searchRecord();
                break;
            case 6:
                updateRecord();
                break;
            case 7:
                deleteRecord();
                break;
            case 8:
                viewSummary();
                break;
            case 9:
                saveRecords();
                printf("\nAll records saved successfully.\n");
                printf("Thank you for using the Electrical Consumption Tracker System!\n");
                printf("Program terminated.\n");
                return 0;
        }
    }

    saveRecords();
    printf("Records saved. Program terminated.\n");
    return 0;
}
