

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// --------------------- Config ---------------------
#define MAX_PATIENTS   1000
#define MAX_DOCTORS     300
#define MAX_APPTS      3000
#define MAX_MEDS        800
#define MAX_INVOICES   6000

#define NAME_LEN        64
#define PHONE_LEN       20
#define ADDR_LEN       128
#define SPEC_LEN        64
#define NOTES_LEN      128
#define DATE_LEN        11   // YYYY-MM-DD
#define TIME_LEN         6   // HH:MM
#define DESC_LEN       128
#define TYPE_LEN        32

#define DELIM           "|"  // pipe-delimited storage

// --------------------- Models ---------------------
typedef struct {
    int id;
    char name[NAME_LEN];
    int age;
    char gender[10];
    char phone[PHONE_LEN];
    char address[ADDR_LEN];
    int admitted;   // 0/1 (kept for future room mgmt)
    int roomNo;     // -1 if none
} Patient;

typedef struct {
    int id;
    char name[NAME_LEN];
    char specialization[SPEC_LEN];
    char phone[PHONE_LEN];
} Doctor;

typedef struct {
    int id;
    int patientId;
    int doctorId;
    char date[DATE_LEN];  // YYYY-MM-DD
    char time[TIME_LEN];  // HH:MM
    char notes[NOTES_LEN];
    int canceled;         // 0/1
} Appointment;

typedef struct {
    int id;
    char name[NAME_LEN];
    int stock;
    double price; // per unit
} Medicine;

typedef struct {
    int id;
    int patientId;
    double amount;
    char description[DESC_LEN];
    char date[DATE_LEN];
} Invoice;

// --------------------- Globals ---------------------
static Patient     g_patients[MAX_PATIENTS];
static int         g_patientCount = 0;
static int         g_nextPatientId = 1;

static Doctor      g_doctors[MAX_DOCTORS];
static int         g_doctorCount = 0;
static int         g_nextDoctorId = 1;

static Appointment g_appts[MAX_APPTS];
static int         g_apptCount = 0;
static int         g_nextApptId = 1;

static Medicine    g_meds[MAX_MEDS];
static int         g_medCount = 0;
static int         g_nextMedId = 1;

static Invoice     g_invoices[MAX_INVOICES];
static int         g_invoiceCount = 0;
static int         g_nextInvoiceId = 1;

// --------------------- Utils ---------------------
/* Portable case-insensitive substring search (replacement for strcasestr).
 * Works on Windows/MSVC and POSIX. Returns a pointer into haystack or NULL.
 */
static const char* strcasestr_portable(const char *haystack, const char *needle){
    if(!haystack || !needle) return NULL;
    if(!*needle) return haystack;
    size_t nlen = strlen(needle);
    for(const char *p = haystack; *p; ++p){
        size_t i = 0;
        while(i < nlen && p[i] &&
              tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])){
            ++i;
        }
        if(i == nlen) return p; // found match starting at p
    }
    return NULL;
}

static void trim_newline(char *s){
    if(!s) return; size_t n=strlen(s); if(n && s[n-1]=='\n') s[n-1]='\0';
}

static void safe_input(const char* prompt, char *buf, size_t bufsz){
    printf("%s", prompt); fflush(stdout);
    if(fgets(buf, (int)bufsz, stdin)==NULL){ buf[0]='\0'; return; }
    trim_newline(buf);
}

static int input_int(const char* prompt){
    char tmp[64];
    while(1){
        safe_input(prompt, tmp, sizeof tmp);
        char *end; long v = strtol(tmp, &end, 10);
        if(end!=tmp && *end=='\0') return (int)v;
        puts("  Invalid integer. Try again.");
    }
}

static double input_double(const char* prompt){
    char tmp[64];
    while(1){
        safe_input(prompt, tmp, sizeof tmp);
        char *end; double v = strtod(tmp, &end);
        if(end!=tmp && *end=='\0') return v;
        puts("  Invalid number. Try again.");
    }
}

static void today(char out[DATE_LEN]){
    time_t t=time(NULL); struct tm *lt=localtime(&t);
    strftime(out, DATE_LEN, "%Y-%m-%d", lt);
}

static void press_enter(){
    printf("\nPress ENTER to continue..."); fflush(stdout);
    int c; while((c=getchar())!='\n' && c!=EOF){}
}

// --------------------- File I/O ---------------------

// Note: We rewrite entire files for simplicity. Strings must not contain '|'.
// If users enter '|', it will be replaced by '/'.
static void sanitize_pipes(char *s){
    for(; *s; ++s) if(*s=='|') *s='/';
}

static void load_patients(){
    FILE *f=fopen("patients.db","r");
    g_patientCount=0; g_nextPatientId=1;
    if(!f) return;
    char line[512];
    while(fgets(line, sizeof line, f)){
        trim_newline(line);
        if(!*line) continue;
        Patient p={0};
        // id|name|age|gender|phone|address|admitted|roomNo
        char name[NAME_LEN],gender[10],phone[PHONE_LEN],addr[ADDR_LEN];
        int id, age, admitted, roomNo;
        if(sscanf(line, "%d|%63[^|]|%d|%9[^|]|%19[^|]|%127[^|]|%d|%d",
                  &id, name, &age, gender, phone, addr, &admitted, &roomNo)==8){
            p.id=id; strncpy(p.name,name,NAME_LEN);
            p.age=age; strncpy(p.gender,gender,sizeof p.gender);
            strncpy(p.phone,phone,PHONE_LEN); strncpy(p.address,addr,ADDR_LEN);
            p.admitted=admitted; p.roomNo=roomNo;
            if(g_patientCount<MAX_PATIENTS) g_patients[g_patientCount++]=p;
            if(id>=g_nextPatientId) g_nextPatientId=id+1;
        }
    }
    fclose(f);
}

static void save_patients(){
    FILE *f=fopen("patients.db","w"); if(!f){perror("patients.db"); return;}
    for(int i=0;i<g_patientCount;i++){
        Patient *p=&g_patients[i];
        char nm[NAME_LEN]; char ph[PHONE_LEN]; char ad[ADDR_LEN]; char gd[10];
        strncpy(nm,p->name,NAME_LEN); sanitize_pipes(nm);
        strncpy(ph,p->phone,PHONE_LEN); sanitize_pipes(ph);
        strncpy(ad,p->address,ADDR_LEN); sanitize_pipes(ad);
        strncpy(gd,p->gender,sizeof gd); sanitize_pipes(gd);
        fprintf(f, "%d|%s|%d|%s|%s|%s|%d|%d\n", p->id, nm, p->age, gd, ph, ad, p->admitted, p->roomNo);
    }
    fclose(f);
}

static void load_doctors(){
    FILE *f=fopen("doctors.db","r"); g_doctorCount=0; g_nextDoctorId=1; if(!f) return;
    char line[512];
    while(fgets(line, sizeof line, f)){
        trim_newline(line); if(!*line) continue; Doctor d={0};
        // id|name|spec|phone
        char name[NAME_LEN], spec[SPEC_LEN], phone[PHONE_LEN]; int id;
        if(sscanf(line, "%d|%63[^|]|%63[^|]|%19[^|]", &id, name, spec, phone)==4){
            d.id=id; strncpy(d.name,name,NAME_LEN); strncpy(d.specialization,spec,SPEC_LEN); strncpy(d.phone,phone,PHONE_LEN);
            if(g_doctorCount<MAX_DOCTORS) g_doctors[g_doctorCount++]=d;
            if(id>=g_nextDoctorId) g_nextDoctorId=id+1;
        }
    }
    fclose(f);
}

static void save_doctors(){
    FILE *f=fopen("doctors.db","w"); if(!f){perror("doctors.db"); return;}
    for(int i=0;i<g_doctorCount;i++){
        Doctor *d=&g_doctors[i]; char nm[NAME_LEN]; char sp[SPEC_LEN]; char ph[PHONE_LEN];
        strncpy(nm,d->name,NAME_LEN); sanitize_pipes(nm);
        strncpy(sp,d->specialization,SPEC_LEN); sanitize_pipes(sp);
        strncpy(ph,d->phone,PHONE_LEN); sanitize_pipes(ph);
        fprintf(f, "%d|%s|%s|%s\n", d->id, nm, sp, ph);
    }
    fclose(f);
}

static void load_appts(){
    FILE *f=fopen("appointments.db","r"); g_apptCount=0; g_nextApptId=1; if(!f) return;
    char line[768];
    while(fgets(line, sizeof line, f)){
        trim_newline(line); if(!*line) continue; Appointment a={0};
        // id|patId|docId|date|time|notes|canceled
        int id, pid, did, canceled; char date[DATE_LEN], tim[TIME_LEN], notes[NOTES_LEN];
        if(sscanf(line, "%d|%d|%d|%10[^|]|%5[^|]|%127[^|]|%d", &id,&pid,&did,date,tim,notes,&canceled)==7){
            a.id=id; a.patientId=pid; a.doctorId=did; strncpy(a.date,date,DATE_LEN); strncpy(a.time,tim,TIME_LEN); strncpy(a.notes,notes,NOTES_LEN); a.canceled=canceled;
            if(g_apptCount<MAX_APPTS) g_appts[g_apptCount++]=a; if(id>=g_nextApptId) g_nextApptId=id+1;
        }
    }
    fclose(f);
}

static void save_appts(){
    FILE *f=fopen("appointments.db","w"); if(!f){perror("appointments.db"); return;}
    for(int i=0;i<g_apptCount;i++){
        Appointment *a=&g_appts[i]; char nt[NOTES_LEN]; strncpy(nt,a->notes,NOTES_LEN); sanitize_pipes(nt);
        fprintf(f, "%d|%d|%d|%s|%s|%s|%d\n", a->id, a->patientId, a->doctorId, a->date, a->time, nt, a->canceled);
    }
    fclose(f);
}

static void load_meds(){
    FILE *f=fopen("medicines.db","r"); g_medCount=0; g_nextMedId=1; if(!f) return;
    char line[512];
    while(fgets(line, sizeof line, f)){
        trim_newline(line); if(!*line) continue; Medicine m={0};
        // id|name|stock|price
        int id, stock; char name[NAME_LEN]; double price;
        if(sscanf(line, "%d|%63[^|]|%d|%lf", &id, name, &stock, &price)==4){
            m.id=id; strncpy(m.name,name,NAME_LEN); m.stock=stock; m.price=price;
            if(g_medCount<MAX_MEDS) g_meds[g_medCount++]=m; if(id>=g_nextMedId) g_nextMedId=id+1;
        }
    }
    fclose(f);
}

static void save_meds(){
    FILE *f=fopen("medicines.db","w"); if(!f){perror("medicines.db"); return;}
    for(int i=0;i<g_medCount;i++){
        Medicine *m=&g_meds[i]; char nm[NAME_LEN]; strncpy(nm,m->name,NAME_LEN); sanitize_pipes(nm);
        fprintf(f, "%d|%s|%d|%.2f\n", m->id, nm, m->stock, m->price);
    }
    fclose(f);
}

static void load_invoices(){
    FILE *f=fopen("invoices.db","r"); g_invoiceCount=0; g_nextInvoiceId=1; if(!f) return;
    char line[768];
    while(fgets(line, sizeof line, f)){
        trim_newline(line); if(!*line) continue; Invoice iv={0};
        // id|patientId|amount|description|date
        int id, pid; double amt; char desc[DESC_LEN], date[DATE_LEN];
        if(sscanf(line, "%d|%d|%lf|%127[^|]|%10[^|]", &id,&pid,&amt,desc,date)==5){
            iv.id=id; iv.patientId=pid; iv.amount=amt; strncpy(iv.description,desc,DESC_LEN); strncpy(iv.date,date,DATE_LEN);
            if(g_invoiceCount<MAX_INVOICES) g_invoices[g_invoiceCount++]=iv; if(id>=g_nextInvoiceId) g_nextInvoiceId=id+1;
        }
    }
    fclose(f);
}

static void save_invoices(){
    FILE *f=fopen("invoices.db","w"); if(!f){perror("invoices.db"); return;}
    for(int i=0;i<g_invoiceCount;i++){
        Invoice *iv=&g_invoices[i]; char ds[DESC_LEN]; strncpy(ds,iv->description,DESC_LEN); sanitize_pipes(ds);
        fprintf(f, "%d|%d|%.2f|%s|%s\n", iv->id, iv->patientId, iv->amount, ds, iv->date);
    }
    fclose(f);
}

// --------------------- Lookups ---------------------
static Patient* find_patient_by_id(int id){
    for(int i=0;i<g_patientCount;i++) if(g_patients[i].id==id) return &g_patients[i];
    return NULL;
}

static Doctor* find_doctor_by_id(int id){
    for(int i=0;i<g_doctorCount;i++) if(g_doctors[i].id==id) return &g_doctors[i];
    return NULL;
}

static Medicine* find_med_by_id(int id){
    for(int i=0;i<g_medCount;i++) if(g_meds[i].id==id) return &g_meds[i];
    return NULL;
}

// --------------------- Patients ---------------------
static void list_patients(){
    printf("\n-- Patients (%d) --\n", g_patientCount);
    printf("%-5s %-20s %-3s %-7s %-14s %-s\n", "ID","Name","Age","Gender","Phone","Address");
    for(int i=0;i<g_patientCount;i++){
        Patient *p=&g_patients[i];
        printf("%-5d %-20.20s %-3d %-7.7s %-14.14s %-40.40s\n", p->id, p->name, p->age, p->gender, p->phone, p->address);
    }
}

static void add_patient(){
    if(g_patientCount>=MAX_PATIENTS){ puts("Patient capacity reached."); return; }
    Patient p={0}; p.id=g_nextPatientId++; p.admitted=0; p.roomNo=-1;
    safe_input("Name: ", p.name, sizeof p.name);
    p.age = input_int("Age: ");
    safe_input("Gender (M/F/Other): ", p.gender, sizeof p.gender);
    safe_input("Phone: ", p.phone, sizeof p.phone);
    safe_input("Address: ", p.address, sizeof p.address);
    g_patients[g_patientCount++]=p; save_patients();
    printf("Added patient with ID %d\n", p.id);
}

static void edit_patient(){
    int id=input_int("Enter patient ID to edit: "); Patient *p=find_patient_by_id(id);
    if(!p){ puts("Not found."); return; }
    char tmp[8];
    printf("Editing patient %d (%s). Leave blank to keep.\n", p->id, p->name);
    char buf[ADDR_LEN];
    safe_input("Name: ", buf, sizeof buf); if(strlen(buf)) strncpy(p->name,buf,sizeof p->name);
    safe_input("Age: ", buf, sizeof buf); if(strlen(buf)) p->age=atoi(buf);
    safe_input("Gender: ", buf, sizeof buf); if(strlen(buf)) strncpy(p->gender,buf,sizeof p->gender);
    safe_input("Phone: ", buf, sizeof buf); if(strlen(buf)) strncpy(p->phone,buf,sizeof p->phone);
    safe_input("Address: ", buf, sizeof buf); if(strlen(buf)) strncpy(p->address,buf,sizeof p->address);
    save_patients(); puts("Updated.");
}

static void delete_patient(){
    int id=input_int("Enter patient ID to delete: "); int idx=-1; for(int i=0;i<g_patientCount;i++) if(g_patients[i].id==id){idx=i;break;}
    if(idx<0){ puts("Not found."); return; }
    for(int i=idx;i<g_patientCount-1;i++) g_patients[i]=g_patients[i+1];
    g_patientCount--; save_patients(); puts("Deleted.");
}

static void search_patient(){
    char name[NAME_LEN]; safe_input("Enter name (partial ok): ", name, sizeof name);
    printf("Results for '%s':\n", name);
    for(int i=0;i<g_patientCount;i++){
        if(strcasestr_portable(g_patients[i].name, name)){
            Patient *p=&g_patients[i];
            printf("  #%d  %s, %d, %s, %s\n", p->id, p->name, p->age, p->gender, p->phone);
        }
    }
}

// --------------------- Doctors ---------------------
static void list_doctors(){
    printf("\n-- Doctors (%d) --\n", g_doctorCount);
    printf("%-5s %-22s %-18s %-14s\n", "ID","Name","Specialization","Phone");
    for(int i=0;i<g_doctorCount;i++){
        Doctor *d=&g_doctors[i];
        printf("%-5d %-22.22s %-18.18s %-14.14s\n", d->id, d->name, d->specialization, d->phone);
    }
}

static void add_doctor(){
    if(g_doctorCount>=MAX_DOCTORS){ puts("Doctor capacity reached."); return; }
    Doctor d={0}; d.id=g_nextDoctorId++;
    safe_input("Name: ", d.name, sizeof d.name);
    safe_input("Specialization: ", d.specialization, sizeof d.specialization);
    safe_input("Phone: ", d.phone, sizeof d.phone);
    g_doctors[g_doctorCount++]=d; save_doctors();
    printf("Added doctor with ID %d\n", d.id);
}

static void edit_doctor(){
    int id=input_int("Enter doctor ID to edit: "); Doctor *d=find_doctor_by_id(id);
    if(!d){ puts("Not found."); return; }
    char buf[128];
    printf("Editing doctor %d (%s). Leave blank to keep.\n", d->id, d->name);
    safe_input("Name: ", buf, sizeof buf); if(strlen(buf)) strncpy(d->name,buf,sizeof d->name);
    safe_input("Specialization: ", buf, sizeof buf); if(strlen(buf)) strncpy(d->specialization,buf,sizeof d->specialization);
    safe_input("Phone: ", buf, sizeof buf); if(strlen(buf)) strncpy(d->phone,buf,sizeof d->phone);
    save_doctors(); puts("Updated.");
}

static void delete_doctor(){
    int id=input_int("Enter doctor ID to delete: "); int idx=-1; for(int i=0;i<g_doctorCount;i++) if(g_doctors[i].id==id){idx=i;break;}
    if(idx<0){ puts("Not found."); return; }
    for(int i=idx;i<g_doctorCount-1;i++) g_doctors[i]=g_doctors[i+1];
    g_doctorCount--; save_doctors(); puts("Deleted.");
}

// --------------------- Appointments ---------------------
static void list_appts(){
    printf("\n-- Appointments (%d) --\n", g_apptCount);
    printf("%-4s %-6s %-6s %-10s %-5s %-s %-s\n", "ID","PatID","DocID","Date","Time","Canceled","Notes");
    for(int i=0;i<g_apptCount;i++){
        Appointment *a=&g_appts[i];
        printf("%-4d %-6d %-6d %-10s %-5s %-7s %-40.40s\n", a->id, a->patientId, a->doctorId, a->date, a->time, a->canceled?"Yes":"No", a->notes);
    }
}

static void schedule_appt(){
    if(g_apptCount>=MAX_APPTS){ puts("Appointment capacity reached."); return; }
    int pid=input_int("Patient ID: "); if(!find_patient_by_id(pid)){ puts("Invalid patient."); return; }
    int did=input_int("Doctor ID: "); if(!find_doctor_by_id(did)){ puts("Invalid doctor."); return; }
    Appointment a={0}; a.id=g_nextApptId++; a.patientId=pid; a.doctorId=did; a.canceled=0;
    safe_input("Date (YYYY-MM-DD): ", a.date, sizeof a.date);
    safe_input("Time (HH:MM): ", a.time, sizeof a.time);
    safe_input("Notes: ", a.notes, sizeof a.notes);
    g_appts[g_apptCount++]=a; save_appts(); puts("Appointment scheduled.");
}

static void cancel_appt(){
    int id=input_int("Appointment ID to cancel: ");
    for(int i=0;i<g_apptCount;i++) if(g_appts[i].id==id){ g_appts[i].canceled=1; save_appts(); puts("Canceled."); return; }
    puts("Not found.");
}

// --------------------- Pharmacy ---------------------
static void list_meds(){
    printf("\n-- Medicines (%d) --\n", g_medCount);
    printf("%-4s %-22s %-8s %-8s\n", "ID","Name","Stock","Price");
    for(int i=0;i<g_medCount;i++){
        Medicine *m=&g_meds[i];
        printf("%-4d %-22.22s %-8d %-8.2f\n", m->id, m->name, m->stock, m->price);
    }
}

static void add_med(){
    if(g_medCount>=MAX_MEDS){ puts("Medicine capacity reached."); return; }
    Medicine m={0}; m.id=g_nextMedId++;
    safe_input("Name: ", m.name, sizeof m.name);
    m.stock = input_int("Initial stock: ");
    m.price = input_double("Price per unit: ");
    g_meds[g_medCount++]=m; save_meds(); printf("Added medicine ID %d\n", m.id);
}

static void restock_med(){
    int id=input_int("Medicine ID: "); Medicine *m=find_med_by_id(id); if(!m){ puts("Not found."); return; }
    int qty=input_int("Add quantity: "); if(qty<0){ puts("Invalid."); return; }
    m->stock += qty; save_meds(); puts("Restocked.");
}

static void sell_med(){
    int pid=input_int("Patient ID: "); Patient *p=find_patient_by_id(pid); if(!p){ puts("Invalid patient."); return; }
    int mid=input_int("Medicine ID: "); Medicine *m=find_med_by_id(mid); if(!m){ puts("Invalid medicine."); return; }

    int qty=input_int("Quantity: "); if(qty<=0){ puts("Invalid quantity."); return; }
    if(qty>m->stock){ puts("Insufficient stock."); return; }
    double total = m->price * qty;
    m->stock -= qty; save_meds();

    // Create invoice
    if(g_invoiceCount>=MAX_INVOICES){ puts("Invoice capacity reached; sale recorded without invoice."); return; }
    Invoice iv={0}; iv.id=g_nextInvoiceId++; iv.patientId=pid; iv.amount=total;
    snprintf(iv.description, sizeof iv.description, "Medicine: %s x %d", m->name, qty);
    today(iv.date);
    g_invoices[g_invoiceCount++]=iv; save_invoices();
    printf("Sold. Invoice #%d Amount: %.2f\n", iv.id, iv.amount);
}

// --------------------- Billing ---------------------
static void list_invoices(){
    printf("\n-- Invoices (%d) --\n", g_invoiceCount);
    printf("%-4s %-6s %-10s %-s\n", "ID","PatID","Amount","Description");
    for(int i=0;i<g_invoiceCount;i++){
        Invoice *iv=&g_invoices[i];
        printf("%-4d %-6d %-10.2f %-40.40s (%s)\n", iv->id, iv->patientId, iv->amount, iv->description, iv->date);
    }
}

static void new_invoice(){
    if(g_invoiceCount>=MAX_INVOICES){ puts("Invoice capacity reached."); return; }
    int pid=input_int("Patient ID: "); if(!find_patient_by_id(pid)){ puts("Invalid patient."); return; }
    double amt=input_double("Amount: ");
    char desc[DESC_LEN]; safe_input("Description: ", desc, sizeof desc);
    Invoice iv={0}; iv.id=g_nextInvoiceId++; iv.patientId=pid; iv.amount=amt; strncpy(iv.description,desc,sizeof iv.description); today(iv.date);
    g_invoices[g_invoiceCount++]=iv; save_invoices(); printf("Invoice created: #%d\n", iv.id);
}

static void patient_balance(){
    int pid=input_int("Patient ID: "); if(!find_patient_by_id(pid)){ puts("Invalid patient."); return; }
    double sum=0.0; for(int i=0;i<g_invoiceCount;i++) if(g_invoices[i].patientId==pid) sum+=g_invoices[i].amount;
    printf("Total billed to patient %d: %.2f\n", pid, sum);
}

// --------------------- Menus ---------------------
static void patients_menu(){
    while(1){
        puts("\n[Patients]\n 1) List\n 2) Add\n 3) Edit\n 4) Delete\n 5) Search by name\n 0) Back");
        int ch=input_int("Choose: ");
        switch(ch){
            case 1: list_patients(); press_enter(); break;
            case 2: add_patient(); press_enter(); break;
            case 3: edit_patient(); press_enter(); break;
            case 4: delete_patient(); press_enter(); break;
            case 5: search_patient(); press_enter(); break;
            case 0: return;
            default: puts("Invalid.");
        }
    }
}

static void doctors_menu(){
    while(1){
        puts("\n[Doctors]\n 1) List\n 2) Add\n 3) Edit\n 4) Delete\n 0) Back");
        int ch=input_int("Choose: ");
        switch(ch){
            case 1: list_doctors(); press_enter(); break;
            case 2: add_doctor(); press_enter(); break;
            case 3: edit_doctor(); press_enter(); break;
            case 4: delete_doctor(); press_enter(); break;
            case 0: return;
            default: puts("Invalid.");
        }
    }
}

static void appts_menu(){
    while(1){
        puts("\n[Appointments]\n 1) List\n 2) Schedule\n 3) Cancel\n 0) Back");
        int ch=input_int("Choose: ");
        switch(ch){
            case 1: list_appts(); press_enter(); break;
            case 2: schedule_appt(); press_enter(); break;
            case 3: cancel_appt(); press_enter(); break;
            case 0: return;
            default: puts("Invalid.");
        }
    }
}

static void pharmacy_menu(){
    while(1){
        puts("\n[Pharmacy]\n 1) List medicines\n 2) Add medicine\n 3) Restock medicine\n 4) Sell medicine (creates invoice)\n 0) Back");
        int ch=input_int("Choose: ");
        switch(ch){
            case 1: list_meds(); press_enter(); break;
            case 2: add_med(); press_enter(); break;
            case 3: restock_med(); press_enter(); break;
            case 4: sell_med(); press_enter(); break;
            case 0: return;
            default: puts("Invalid.");
        }
    }
}

static void billing_menu(){
    while(1){
        puts("\n[Billing]\n 1) List invoices\n 2) New invoice (manual)\n 3) Patient total billed\n 0) Back");
        int ch=input_int("Choose: ");
        switch(ch){
            case 1: list_invoices(); press_enter(); break;
            case 2: new_invoice(); press_enter(); break;
            case 3: patient_balance(); press_enter(); break;
            case 0: return;
            default: puts("Invalid.");
        }
    }
}

// --------------------- Main ---------------------
static void load_all(){
    load_patients();
    load_doctors();
    load_appts();
    load_meds();
    load_invoices();
}

int main(void){
    load_all();
    puts("\n=== Hospital Management System (C) ===");
    for(;;){
        puts("\nMain Menu\n 1) Patients\n 2) Doctors\n 3) Appointments\n 4) Pharmacy\n 5) Billing\n 9) About\n 0) Exit");
        int ch=input_int("Choose: ");
        switch(ch){
            case 1: patients_menu(); break;
            case 2: doctors_menu(); break;
            case 3: appts_menu(); break;
            case 4: pharmacy_menu(); break;
            case 5: billing_menu(); break;
            case 9: puts("Simple text-file HMS. Extend as you like. Developed as a learning project."); press_enter(); break;
            case 0: puts("Goodbye!"); return 0;
            default: puts("Invalid choice.");
        }
    }
}
