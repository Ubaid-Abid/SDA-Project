#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>

using namespace std;

//took a lot of efforts
//apparently SonarQube would still give 57 warnings saying use the better pointers etc and something on class inheritance as well, basically modern cpp
// but, that would require whole change in code
// automatic memory handling isn't necessary but understandable why SonarQube says otherwise

static string getCurrentYearMonth() //static for a reason...
{
    //this is to handle the current time and date etc.
    time_t t = time(nullptr);
    tm* now = localtime(&t);
    int year  = now->tm_year + 1900;
    int month = now->tm_mon  + 1;
    ostringstream oss;
    oss << year << "-" << (month < 10 ? "0" : "") << month;
    return oss.str();
}

class Claim;
class Person;
class Customer;
class PersonFactory;

class ClaimState
{
public:
    virtual ~ClaimState() = default;

    virtual string getStatusName() const = 0;

    virtual void processSurvey(Claim* context) const
    {
        cout << "Action not allowed in current state (" << getStatusName() << ").\n";
    }

    virtual void processApproval(Claim* context, bool approved) const
    {
        cout << "Action not allowed in current state (" << getStatusName() << ").\n";
    }
};

class PendingInspection : public ClaimState
{
public:
    string getStatusName() const override { return "Pending Inspection"; }
    void processSurvey(Claim* context) const override;
};

class PendingApproval : public ClaimState
{
public:
    string getStatusName() const override { return "Pending Approval"; }
    void processApproval(Claim* context, bool approved) const override;
};

class ApprovedState : public ClaimState
{
public:
    string getStatusName() const override { return "Approved"; }
};

class RejectedState : public ClaimState
{
public:
    string getStatusName() const override { return "Rejected"; }
};

class Claim
{
private:
    ClaimState* state;

public:
    int id;
    int customerId;
    int policyId;
    string description;
    int assignedWorkshopId;
    string inspectionNotes;

    explicit Claim(int i, int cid, int pid, const string& desc,
                   int workshopId = -1, const string& notes = "")
        : state(new PendingInspection()), id(i), customerId(cid), policyId(pid),
          description(desc), assignedWorkshopId(workshopId), inspectionNotes(notes)
    {
    }

    ~Claim() { delete state; }

    Claim(const Claim&) = delete;
    Claim& operator=(const Claim&) = delete;

    void setState(ClaimState* newState) { delete state; state = newState; }

    void forceState(const string& stateName)
    {
        delete state;
        if (stateName == "Pending Approval")      state = new PendingApproval();
        else if (stateName == "Approved")          state = new ApprovedState();
        else if (stateName == "Rejected")          state = new RejectedState();
        else                                       state = new PendingInspection();
    }

    string getStatus() const       { return state->getStatusName(); }
    void submitSurvey()            { state->processSurvey(this); }
    void evaluateApproval(bool approved) { state->processApproval(this, approved); }
};

void PendingInspection::processSurvey(Claim* context) const
{
    context->setState(new PendingApproval());
    cout << "Inspection submitted. Status updated to Pending Approval.\n";
}

void PendingApproval::processApproval(Claim* context, bool approved) const
{
    if (approved)
    {
        context->setState(new ApprovedState());
        cout << "Claim Approved.\n";
    }
    else
    {
        context->setState(new RejectedState());
        cout << "Claim Rejected.\n";
    }
}

class Vehicle
{
public:
    int id;
    int ownerId;
    string make;
    string model;

    explicit Vehicle(int i, int o, const string& mk, const string& md)
        : id(i), ownerId(o), make(mk), model(md)
    {
    }
};

class Policy
{
public:
    int id;
    int customerId;
    int vehicleId;

    explicit Policy(int i, int cid, int vid)
        : id(i), customerId(cid), vehicleId(vid)
    {
    }
};

class Workshop
{
public:
    int id;
    string name;
    bool isRegistered;

    explicit Workshop(int i, const string& n, bool reg)
        : id(i), name(n), isRegistered(reg)
    {
    }
};

class DatabaseContext
{
public:
    vector<Vehicle*>  vehicles;
    vector<Policy*>   policies;
    vector<Claim*>    claims;
    vector<Workshop*> workshops;
    map<int, Person*> users;

    int nextUserId;
    int nextVehicleId;
    int nextPolicyId;
    int nextClaimId;
    int nextWorkshopId;

    DatabaseContext()
        : nextUserId(1000), nextVehicleId(1), nextPolicyId(1),
          nextClaimId(1), nextWorkshopId(1)
    {
    }

    ~DatabaseContext();

    DatabaseContext(const DatabaseContext&) = delete;
    DatabaseContext& operator=(const DatabaseContext&) = delete;

    bool isCustomer(int id) const;

    bool hasVehicle(int vId, int cId) const
    {
        for (Vehicle* v : vehicles)
        {
            if (v->id == vId && v->ownerId == cId) return true;
        }
        return false;
    }

    bool isValidPolicyForCustomer(int pId, int cId) const
    {
        for (Policy* p : policies)
        {
            if (p->id == pId && p->customerId == cId) return true;
        }
        return false;
    }
};

class Person
{
private:
    int    id;
    string name;
    string password;

protected:
    virtual void printMenu() const = 0;
    virtual void handleChoice(int choice, DatabaseContext& db) = 0;

public:
    explicit Person(int i, const string& n, const string& pw)
        : id(i), name(n), password(pw)
    {
    }

    virtual ~Person() = default;

    int    getId()       const { return id; }
    string getName()     const { return name; }
    string getPassword() const { return password; }

    void setPassword(const string& pw)         { password = pw; }
    bool checkPassword(const string& pw) const { return password == pw; }

    virtual int getRoleType() const = 0;

    void executeRoleMenu(DatabaseContext& db)
    {
        int choice = -1;
        while (choice != 0)
        {
            printMenu();
            if (!(cin >> choice))
            {
                cin.clear();
                cin.ignore(1000, '\n');
                choice = -1;
                continue;
            }
            if (choice == 0) break;
            handleChoice(choice, db);
        }
    }
};

class Customer : public Person
{
private:
    string joinDate; 

    void registerVehicle(DatabaseContext& db) const
    {
        string mk, md;
        cin.ignore(1000, '\n');
        cout << "Make: ";
        getline(cin, mk);
        cout << "Model: ";
        getline(cin, md);
        Vehicle* v = new Vehicle(db.nextVehicleId++, getId(), mk, md);
        db.vehicles.push_back(v);
        cout << "Vehicle Registered. ID: " << v->id << "\n";
    }

    void viewVehicles(const DatabaseContext& db) const
    {
        cout << "\n--- My Vehicles ---\n";
        for (Vehicle* v : db.vehicles)
        {
            if (v->ownerId == getId())
                cout << "ID: " << v->id << " | Make: " << v->make
                     << " | Model: " << v->model << "\n";
        }
    }

    void viewPolicies(const DatabaseContext& db) const
    {
        cout << "\n--- My Policies ---\n";
        for (Policy* p : db.policies)
        {
            if (p->customerId == getId())
                cout << "Policy ID: " << p->id
                     << " | Covers Vehicle ID: " << p->vehicleId << "\n";
        }
    }

    void fileClaim(DatabaseContext& db) const
    {
        int pId;
        cout << "Policy ID: ";
        cin >> pId;
        cin.ignore(1000, '\n');

        if (db.isValidPolicyForCustomer(pId, getId()))
        {
            string desc;
            cout << "Description of incident: ";
            getline(cin, desc);
            Claim* c = new Claim(db.nextClaimId++, getId(), pId, desc);
            db.claims.push_back(c);
            cout << "Claim Filed. ID: " << c->id << "\n";
        }
        else
        {
            cout << "Error: Invalid Policy ID.\n";
        }
    }

    void viewClaims(const DatabaseContext& db) const
    {
        cout << "\n--- My Claims ---\n";
        for (Claim* c : db.claims)
        {
            if (c->customerId == getId())
                cout << "Claim " << c->id << ": " << c->getStatus() << "\n";
        }
    }

protected:
    void printMenu() const override
    {
        cout << "\n--- Customer Menu (" << getName() << ") ---\n"
             << "1. Register Vehicle\n2. View My Vehicles\n3. View My Policies\n"
             << "4. File Claim\n5. View Claim Status\n0. Logout\nChoice: ";
    }

    void handleChoice(int choice, DatabaseContext& db) override
    {
        switch (choice)
        {
            case 1: registerVehicle(db); break;
            case 2: viewVehicles(db);    break;
            case 3: viewPolicies(db);    break;
            case 4: fileClaim(db);       break;
            case 5: viewClaims(db);      break;
            default: break;
        }
    }

public:
    explicit Customer(int i, const string& n, const string& pw, const string& jd = "")
        : Person(i, n, pw), joinDate(jd.empty() ? getCurrentYearMonth() : jd)
    {
    }

    string getJoinDate() const   { return joinDate; }
    int getRoleType() const override { return 1; }
};

bool DatabaseContext::isCustomer(int id) const
{
    map<int, Person*>::const_iterator it = users.find(id);
    if (it != users.end())
        return dynamic_cast<Customer*>(it->second) != nullptr;
    return false;
}

DatabaseContext::~DatabaseContext()
{
    for (Vehicle* v  : vehicles)  delete v;
    for (Policy* p   : policies)  delete p;
    for (Claim* c    : claims)    delete c;
    for (Workshop* w : workshops) delete w;
    map<int, Person*>::iterator it;
    for (it = users.begin(); it != users.end(); ++it) delete it->second;
}

class Salesman : public Person
{
private:
    void issuePolicy(DatabaseContext& db) const
    {
        int cId, vId;
        cout << "Customer ID: ";
        cin >> cId;
        if (!db.isCustomer(cId)) { cout << "Error: Invalid Customer ID.\n"; return; }
        cout << "Vehicle ID: ";
        cin >> vId;
        if (!db.hasVehicle(vId, cId)) { cout << "Error: Invalid Vehicle ID.\n"; return; }
        Policy* p = new Policy(db.nextPolicyId++, cId, vId);
        db.policies.push_back(p);
        cout << "Policy Issued. ID: " << p->id << "\n";
    }

protected:
    void printMenu() const override
    {
        cout << "\n--- Salesman Menu ---\n1. Issue Policy\n0. Logout\nChoice: ";
    }

    void handleChoice(int choice, DatabaseContext& db) override
    {
        if (choice == 1) issuePolicy(db);
    }

public:
    explicit Salesman(int i, const string& n, const string& pw) : Person(i, n, pw) {}
    int getRoleType() const override { return 3; }
};

class Surveyor : public Person
{
private:
    void inspectClaims(DatabaseContext& db) const
    {
        cout << "\n--- Claims Pending Inspection ---\n";
        bool found = false;
        for (Claim* c : db.claims)
        {
            if (c->getStatus() == "Pending Inspection")
            {
                cout << "Claim ID: " << c->id << " | Policy: " << c->policyId
                     << "\nDescription: " << c->description << "\n---\n";
                found = true;
            }
        }
        if (!found) { cout << "No claims pending inspection.\n"; return; }

        int cId;
        cout << "Enter Claim ID to inspect (or 0 to cancel): ";
        cin >> cId;
        if (cId == 0) return;

        for (Claim* c : db.claims)
        {
            if (c->id == cId && c->getStatus() == "Pending Inspection")
            {
                cin.ignore(1000, '\n');
                cout << "Enter inspection notes/report: ";
                getline(cin, c->inspectionNotes);
                c->submitSurvey();
                return;
            }
        }
        cout << "Invalid Claim ID or claim not pending inspection.\n";
    }

protected:
    void printMenu() const override
    {
        cout << "\n--- Surveyor Menu ---\n1. View & Inspect Claims\n0. Logout\nChoice: ";
    }

    void handleChoice(int choice, DatabaseContext& db) override
    {
        if (choice == 1) inspectClaims(db);
    }

public:
    explicit Surveyor(int i, const string& n, const string& pw) : Person(i, n, pw) {}
    int getRoleType() const override { return 4; }
};

class Manager : public Person
{
private:
    void evaluatePendingClaims(DatabaseContext& db) const
    {
        cout << "\n--- Claims Pending Approval ---\n";
        for (Claim* c : db.claims)
        {
            if (c->getStatus() == "Pending Approval")
            {
                cout << "Claim ID: " << c->id << " | Desc: " << c->description
                     << "\nInspection Notes: " << c->inspectionNotes << "\n---\n";
            }
        }

        int cId;
        cout << "Enter Claim ID to evaluate: ";
        cin >> cId;
        Claim* target = nullptr;
        for (Claim* c : db.claims)
        {
            if (c->id == cId) { target = c; break; }
        }

        if (!target || target->getStatus() != "Pending Approval")
        {
            cout << "Invalid Claim ID.\n";
            return;
        }

        int dec;
        cout << "1. Approve\n2. Reject\nChoice: ";
        cin >> dec;
        bool isApp = (dec == 1);
        target->evaluateApproval(isApp);

        if (isApp)
        {
            cout << "Registered Workshops:\n";
            for (Workshop* w : db.workshops)
            {
                if (w->isRegistered)
                    cout << w->id << ". " << w->name << "\n";
            }
            cout << "Assign Workshop ID: ";
            cin >> target->assignedWorkshopId;
        }
    }

    void viewCustomerHistory(const DatabaseContext& db) const
    {
        int custId;
        cout << "Customer ID: ";
        cin >> custId;
        bool found = false;
        for (Claim* c : db.claims)
        {
            if (c->customerId == custId)
            {
                cout << "Claim " << c->id << ": " << c->getStatus() << "\n";
                found = true;
            }
        }
        if (!found) cout << "No claims found for this customer.\n";
    }

    void reportNewCustomers(const DatabaseContext& db) const
    {
        string month;
        cin.ignore(1000, '\n');
        cout << "Enter month (YYYY-MM) or press Enter for current month: ";
        getline(cin, month);
        if (month.empty()) month = getCurrentYearMonth();

        cout << "\n--- New Customers in " << month << " ---\n";
        bool found = false;
        for (auto& kv : db.users)
        {
            Customer* c = dynamic_cast<Customer*>(kv.second);
            if (c && c->getJoinDate() == month)
            {
                cout << "ID: " << c->getId() << " | Name: " << c->getName() << "\n";
                found = true;
            }
        }
        if (!found) cout << "No new customers found in this month.\n";
    }

    void reportPendingClaims(const DatabaseContext& db) const
    {
        cout << "\n--- Pending Claims Report ---\n";
        bool found = false;
        for (Claim* c : db.claims)
        {
            if (c->getStatus() == "Pending Inspection" || c->getStatus() == "Pending Approval")
            {
                cout << "Claim ID: " << c->id
                     << " | Customer: " << c->customerId
                     << " | Status: " << c->getStatus() << "\n";
                found = true;
            }
        }
        if (!found) cout << "No pending claims.\n";
    }

    
    void reportInspections(const DatabaseContext& db) const
    {
        cout << "\n--- Inspection Reports ---\n";
        bool found = false;
        for (Claim* c : db.claims)
        {
            if (!c->inspectionNotes.empty())
            {
                cout << "Claim ID: " << c->id
                     << " | Customer: " << c->customerId
                     << " | Status: " << c->getStatus()
                     << "\nInspection Notes: " << c->inspectionNotes << "\n---\n";
                found = true;
            }
        }
        if (!found) cout << "No inspection reports available yet.\n";
    }

protected:
    void printMenu() const override
    {
        cout << "\n--- Manager Menu ---\n"
             << "1. Evaluate Claims\n"
             << "2. Customer History\n"
             << "3. Report: New Customers in Month\n"
             << "4. Report: Pending Claims\n"
             << "5. Report: Inspection Reports\n"
             << "0. Logout\nChoice: ";
    }

    void handleChoice(int choice, DatabaseContext& db) override
    {
        switch (choice)
        {
            case 1: evaluatePendingClaims(db);  break;
            case 2: viewCustomerHistory(db);    break;
            case 3: reportNewCustomers(db);     break;
            case 4: reportPendingClaims(db);    break;
            case 5: reportInspections(db);      break;
            default: break;
        }
    }

public:
    explicit Manager(int i, const string& n, const string& pw) : Person(i, n, pw) {}
    int getRoleType() const override { return 2; }
};

class Admin : public Person
{
private:
    void addWorkshop(DatabaseContext& db) const
    {
        cin.ignore(1000, '\n');
        string name;
        cout << "Workshop Name: ";
        getline(cin, name);
        int id = db.nextWorkshopId++;
        db.workshops.push_back(new Workshop(id, name, true));
        cout << "Workshop added. ID: " << id << "\n";
    }

    void removeWorkshop(DatabaseContext& db) const
    {
        int wId;
        cout << "Workshop ID to remove: ";
        cin >> wId;
        for (auto it = db.workshops.begin(); it != db.workshops.end(); ++it)
        {
            if ((*it)->id == wId)
            {
                delete *it;
                db.workshops.erase(it);
                cout << "Workshop removed.\n";
                return;
            }
        }
        cout << "Workshop not found.\n";
    }

    void listWorkshops(const DatabaseContext& db) const
    {
        cout << "\n--- All Workshops ---\n";
        for (Workshop* w : db.workshops)
            cout << "ID: " << w->id << " | Name: " << w->name
                 << " | Registered: " << (w->isRegistered ? "Yes" : "No") << "\n";
    }

    void addUser(DatabaseContext& db) const; 

    void removeUser(DatabaseContext& db) const
    {
        int uid;
        cout << "User ID to remove: ";
        cin >> uid;
        if (uid == 1) { cout << "Cannot remove the Admin account.\n"; return; }
        auto it = db.users.find(uid);
        if (it != db.users.end())
        {
            delete it->second;
            db.users.erase(it);
            cout << "User removed.\n";
        }
        else
        {
            cout << "User not found.\n";
        }
    }

    void listUsers(const DatabaseContext& db) const
    {
        const string roleNames[] = {"Admin", "Customer", "Manager", "Salesman", "Surveyor"};
        cout << "\n--- All Users ---\n";
        for (auto& kv : db.users)
        {
            int role = kv.second->getRoleType();
            string roleName = (role >= 0 && role <= 4) ? roleNames[role] : "Unknown";
            cout << "ID: " << kv.first << " | Name: " << kv.second->getName()
                 << " | Role: " << roleName << "\n";
        }
    }

protected:
    void printMenu() const override
    {
        cout << "\n--- Admin Menu ---\n"
             << "1. Add Workshop\n2. Remove Workshop\n3. List Workshops\n"
             << "4. Add User\n5. Remove User\n6. List All Users\n"
             << "0. Logout\nChoice: ";
    }

    void handleChoice(int choice, DatabaseContext& db) override
    {
        switch (choice)
        {
            case 1: addWorkshop(db);   break;
            case 2: removeWorkshop(db); break;
            case 3: listWorkshops(db); break;
            case 4: addUser(db);       break;
            case 5: removeUser(db);    break;
            case 6: listUsers(db);     break;
            default: break;
        }
    }

public:
    explicit Admin(int i, const string& n, const string& pw) : Person(i, n, pw) {}
    int getRoleType() const override { return 0; }
};

class PersonFactory
{
public:
    static Person* createPerson(int type, int id, const string& name,
                                const string& pw, const string& extra = "")
    {
        switch (type)
        {
            case 0: return new Admin(id, name, pw);
            case 1: return new Customer(id, name, pw, extra); // extra = joinDate
            case 2: return new Manager(id, name, pw);
            case 3: return new Salesman(id, name, pw);
            case 4: return new Surveyor(id, name, pw);
            default: return nullptr;
        }
    }
};


void Admin::addUser(DatabaseContext& db) const
{
    int type;
    cout << "Role (1.Customer, 2.Manager, 3.Salesman, 4.Surveyor): ";
    cin >> type;
    if (type < 1 || type > 4) { cout << "Invalid role.\n"; return; }

    string name, pw;
    cin.ignore(1000, '\n');
    cout << "Name: ";
    getline(cin, name);
    cout << "Password: ";
    getline(cin, pw);

    int id = db.nextUserId++;
    Person* p = PersonFactory::createPerson(type, id, name, pw);
    if (p)
    {
        db.users[id] = p;
        cout << "User added. ID: " << id << "\n";
    }
}

class IDataStorage
{
public:
    virtual void save(const DatabaseContext& db) const = 0;
    virtual void load(DatabaseContext& db) = 0;
    virtual ~IDataStorage() = default;
};

class TextFileStorage : public IDataStorage
{
private:
    const string filename = "insurance_data.txt";

    vector<string> split(const string& s, char delimiter) const
    {
        vector<string> tokens;
        string token;
        istringstream tokenStream(s);
        while (getline(tokenStream, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }

public:
    void save(const DatabaseContext& db) const override
    {
        ofstream out(filename.c_str());
        if (!out) return;

        for (Workshop* w : db.workshops)
            out << "WORKSHOP|" << w->id << "|" << w->name
                << "|" << (w->isRegistered ? 1 : 0) << "\n";

        for (auto& kv : db.users)
        {
            Person* p = kv.second;
            out << "USER|" << p->getRoleType() << "|" << p->getId()
                << "|" << p->getName() << "|" << p->getPassword();
            if (p->getRoleType() == 1)
            {
                Customer* c = dynamic_cast<Customer*>(p);
                out << "|" << c->getJoinDate();
            }
            out << "\n";
        }

    
        for (Vehicle* v : db.vehicles)
            out << "VEHICLE|" << v->id << "|" << v->ownerId
                << "|" << v->make << "|" << v->model << "\n";

   
        for (Policy* p : db.policies)
            out << "POLICY|" << p->id << "|" << p->customerId
                << "|" << p->vehicleId << "\n";

    
        for (Claim* c : db.claims)
            out << "CLAIM|" << c->id << "|" << c->customerId << "|" << c->policyId
                << "|" << c->description << "|" << c->assignedWorkshopId
                << "|" << c->getStatus() << "|" << c->inspectionNotes << "\n";

        out.close();
    }

    void load(DatabaseContext& db) override
    {
        ifstream in(filename.c_str());
        if (!in) return;

        string line;
        while (getline(in, line))
        {
            vector<string> t = split(line, '|');
            if (t.empty()) continue;

            if (t[0] == "WORKSHOP" && t.size() >= 4)
            {
                int id  = stoi(t[1]);
                bool reg = (stoi(t[3]) == 1);
                db.workshops.push_back(new Workshop(id, t[2], reg));
                db.nextWorkshopId = max(db.nextWorkshopId, id + 1);
            }
            else if (t[0] == "USER" && t.size() >= 5)
            {
                int role = stoi(t[1]);
                int id   = stoi(t[2]);
                string nm = t[3];
                string pw = t[4];
                string extra = (t.size() >= 6) ? t[5] : "";
                Person* p = PersonFactory::createPerson(role, id, nm, pw, extra);
                if (p)
                {
                    db.users[id] = p;
                    db.nextUserId = max(db.nextUserId, id + 1);
                }
            }
            else if (t[0] == "VEHICLE" && t.size() >= 5)
            {
                int id = stoi(t[1]);
                db.vehicles.push_back(new Vehicle(id, stoi(t[2]), t[3], t[4]));
                db.nextVehicleId = max(db.nextVehicleId, id + 1);
            }
            else if (t[0] == "POLICY" && t.size() >= 4)
            {
                int id = stoi(t[1]);
                db.policies.push_back(new Policy(id, stoi(t[2]), stoi(t[3])));
                db.nextPolicyId = max(db.nextPolicyId, id + 1);
            }
            else if (t[0] == "CLAIM" && t.size() >= 7)
            {
                int id  = stoi(t[1]);
                string notes = (t.size() >= 8) ? t[7] : "";
                Claim* c = new Claim(id, stoi(t[2]), stoi(t[3]), t[4], stoi(t[5]), notes);
                c->forceState(t[6]);
                db.claims.push_back(c);
                db.nextClaimId = max(db.nextClaimId, id + 1);
            }
        }
        in.close();
    }
};


class InsuranceSystem
{
private:
    DatabaseContext db;
    IDataStorage*   storage;

    void registerUser(int type)
    {
        string name, pw;
        cin.ignore(1000, '\n');
        cout << "Name: ";
        getline(cin, name);
        cout << "Password: ";
        getline(cin, pw);
        int id = db.nextUserId++;
        Person* p = PersonFactory::createPerson(type, id, name, pw);
        if (p)
        {
            db.users[id] = p;
            cout << "Registered successfully. Your Login ID: " << id << "\n";
            storage->save(db);
        }
    }

public:
    explicit InsuranceSystem(IDataStorage* s) : storage(s)
    {
        storage->load(db);

        // Seed Admin (ID = 1) if not already in file
        if (db.users.find(1) == db.users.end())
        {
            db.users[1] = new Admin(1, "Admin", "admin123");
            cout << "=== Default Admin created ===\n"
                 << "    Login ID : 1\n"
                 << "    Password : admin123\n";
        }

        if (db.workshops.empty())
        {
            db.workshops.push_back(new Workshop(db.nextWorkshopId++, "FixIt_Center",    true));
            db.workshops.push_back(new Workshop(db.nextWorkshopId++, "Elite_Auto_Care", true));
        }

        storage->save(db);
    }

    ~InsuranceSystem()
    {
        storage->save(db);
        delete storage;
    }

    InsuranceSystem(const InsuranceSystem&) = delete;
    InsuranceSystem& operator=(const InsuranceSystem&) = delete;

    void run()
    {
        int choice = -1;
        while (choice != 0)
        {
            cout << "\n=== Automobile Insurance System ===\n"
                 << "1. Login\n2. Register as Customer\n3. Register Staff\n"
                 << "0. Exit\nChoice: ";

            if (!(cin >> choice))
            {
                cin.clear();
                cin.ignore(1000, '\n');
                continue;
            }

            if (choice == 1)
            {
                int id;
                string pw;
                cout << "User ID: ";
                cin >> id;
                cout << "Password: ";
                cin >> pw;

                auto it = db.users.find(id);
                if (it != db.users.end() && it->second->checkPassword(pw))
                {
                    cout << "Welcome, " << it->second->getName() << "!\n";
                    it->second->executeRoleMenu(db);
                    storage->save(db);
                }
                else
                {
                    cout << "Invalid ID or password.\n";
                }
            }
            else if (choice == 2)
            {
                registerUser(1);
            }
            else if (choice == 3)
            {
                int st;
                cout << "Staff Role (2.Manager, 3.Salesman, 4.Surveyor): ";
                cin >> st;
                if (st >= 2 && st <= 4)
                    registerUser(st);
                else
                    cout << "Invalid role.\n";
            }
        }
        cout << "Goodbye!\n";
    }
};

int main()
{
    InsuranceSystem sys(new TextFileStorage());
    sys.run();
    return 0;
}