"""Regenerates TeamForge's local sample data (data/students.json, requirements.json, teams.json,
skills.json and interest_requests.json).

The first 12 records of the existing students.json are kept (the four TeamForge members and
eight early profiles); generated participant profiles are appended until there are
TOTAL_STUDENTS. Generation is seeded, so re-running produces identical files. Generated people
are not real students; the JSON note says so and the UI never labels them.

interest_requests.json seeds the interest workflow: participants who expressed interest in each
project, at mixed review stages. TF-P001 (Anubhav Bisht) is the demo participant.

skills.json is the skill catalogue: every skill in CATEGORIES (lower-cased, as stored) with its
category's display label from CATEGORY_LABELS, sorted by name. Every catalogue skill is offered
by at least one generated participant. Skills stay open-ended; the catalogue only labels the
known ones.

    python tools/generate_sample_data.py
"""

import json
import random
from collections import Counter
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / "data"
TOTAL_STUDENTS = 560
SEED = 2026
MIN_DISTINCT_SKILLS = 500

# Skill universe by category, in display spelling. Stored names are the lower-cased form (the
# core normalises them the same way); the app is open-ended, this is only the initial data.
CATEGORIES = {
    "programming": ["Python", "Java", "C", "C++", "C#", "JavaScript", "TypeScript", "Go", "Rust", "Kotlin",
                    "Swift", "Dart", "PHP", "Ruby", "Scala", "R", "MATLAB", "Julia", "Bash", "Haskell", "Lua", "Perl",
                    "Assembly language", "Objective-C", "PowerShell", "Elixir", "Fortran", "Clojure"],
    "web": ["HTML/CSS", "React", "Angular", "Vue.js", "Next.js", "Node.js", "Express.js", "Django", "Flask",
            "FastAPI", "Spring Boot", "Tailwind CSS", "GraphQL", "REST API design", "web accessibility", "Svelte",
            "ASP.NET Core", "Laravel", "web performance", "WebSockets", "microservices", "WebAssembly",
            "Bootstrap", "Redux", "Sass", "Ruby on Rails", "NestJS", "progressive web apps", "OAuth", "gRPC",
            "WordPress", "Three.js"],
    "mobile": ["Android", "Flutter", "React Native", "Jetpack Compose", "SwiftUI", "iOS development", "Firebase",
               "mobile UI", "Kotlin Multiplatform", "app store deployment", "push notifications", "mobile testing",
               "Android Studio", "Xcode", "Expo", "Ionic", ".NET MAUI", "offline-first apps"],
    "ai": ["machine learning", "deep learning", "computer vision", "NLP", "PyTorch", "TensorFlow", "Keras",
           "scikit-learn", "reinforcement learning", "generative AI", "LLM fine-tuning", "RAG", "LangChain",
           "prompt engineering", "Hugging Face", "MLOps", "edge AI", "OpenCV", "speech recognition",
           "recommender systems", "time series forecasting", "model evaluation", "data annotation",
           "large language models", "transformers", "feature engineering", "transfer learning", "object detection",
           "image segmentation", "XGBoost", "AI agents", "anomaly detection", "sentiment analysis", "OCR",
           "explainable AI", "AI ethics"],
    "data": ["data analysis", "data visualization", "pandas", "NumPy", "statistics", "Excel", "Power BI", "Tableau",
             "SQL", "data engineering", "Apache Spark", "Kafka", "Airflow", "ETL pipelines", "data cleaning",
             "web scraping", "A/B testing", "dbt", "Jupyter", "big data", "Matplotlib", "Seaborn", "Plotly",
             "exploratory data analysis", "regression analysis", "hypothesis testing", "data warehousing",
             "BigQuery", "Databricks", "Hadoop"],
    "databases": ["databases", "PostgreSQL", "MySQL", "MongoDB", "Redis", "SQLite", "Firestore", "Cassandra",
                  "Elasticsearch", "Neo4j", "database design", "query optimization", "NoSQL", "Microsoft SQL Server",
                  "Oracle Database", "DynamoDB", "Supabase", "vector databases"],
    "cloud": ["cloud", "AWS", "Azure", "Google Cloud", "serverless", "cloud architecture", "Terraform", "Cloudflare",
              "AWS Lambda", "Amazon EC2", "Amazon S3", "Azure Functions", "Vercel", "edge computing",
              "CloudFormation", "cloud migration"],
    "devops": ["Docker", "Kubernetes", "CI/CD", "GitHub Actions", "Jenkins", "Ansible", "Linux", "Git", "monitoring",
               "Prometheus", "Nginx", "DevOps", "Helm", "Maven", "Gradle", "Grafana", "GitLab CI",
               "infrastructure as code", "site reliability engineering", "ELK stack", "Argo CD", "GitOps", "CMake"],
    "security": ["cybersecurity", "network security", "penetration testing", "cryptography", "ethical hacking",
                 "web security", "malware analysis", "digital forensics", "OWASP", "security auditing", "IAM", "CTF",
                 "vulnerability assessment", "incident response", "threat modelling", "Burp Suite", "Metasploit",
                 "Nmap", "SIEM", "reverse engineering", "secure coding", "cloud security"],
    "networking": ["networking", "TCP/IP", "Wireshark", "SDN", "5G networks", "network administration",
                   "routing and switching", "socket programming", "DNS", "VPN", "load balancing", "subnetting",
                   "Cisco Packet Tracer", "wireless networks"],
    "systems": ["DSA", "algorithms", "OOP", "design patterns", "system design", "operating systems", "compilers",
                "distributed systems", "multithreading", "Qt", "QML", "CUDA", "parallel computing",
                "competitive programming", "computer graphics", "OpenGL", "computer architecture", "Linux kernel",
                "memory management", "systems programming", "debugging", "GDB", "performance profiling",
                "functional programming", "UML"],
    "embedded": ["embedded C", "Arduino", "Raspberry Pi", "ESP32", "STM32", "RTOS", "FPGA", "Verilog", "VHDL",
                 "firmware development", "microcontrollers", "embedded Linux", "ARM Cortex-M", "I2C", "SPI", "UART",
                 "CAN bus", "device drivers", "SystemVerilog"],
    "iot": ["IoT", "MQTT", "sensor networks", "LoRa", "home automation", "IoT security", "Node-RED",
            "Bluetooth Low Energy", "Zigbee", "industrial IoT", "AWS IoT", "digital twins", "Modbus"],
    "robotics": ["ROS2", "robotics", "SLAM", "motion planning", "drone programming", "robot kinematics", "Gazebo",
                 "control systems", "PID control", "sensor fusion", "Kalman filters", "LiDAR", "autonomous vehicles",
                 "robot perception"],
    "electronics": ["electronics", "PCB design", "circuit design", "KiCad", "signal processing", "power electronics",
                    "analog electronics", "soldering", "digital electronics", "VLSI design", "LTspice", "Proteus",
                    "Altium Designer", "motor control"],
    "mechanical": ["SolidWorks", "AutoCAD", "ANSYS", "Fusion 360", "CAD modelling", "3D printing",
                   "finite element analysis", "CATIA", "GD&T", "Creo", "CNC machining", "thermodynamics",
                   "fluid mechanics", "heat transfer", "machine design"],
    "design": ["UI design", "UX design", "Figma", "Adobe XD", "user research", "prototyping", "wireframing",
               "design systems", "interaction design", "accessibility design", "motion design", "illustration",
               "graphic design", "branding", "typography", "usability testing", "design thinking",
               "information architecture", "user flows", "visual design", "Photoshop", "Illustrator",
               "responsive design", "UX writing"],
    "game": ["Unity", "Unreal Engine", "game design", "Blender", "3D modelling", "AR/VR development",
             "shader programming", "Godot", "level design", "game physics", "procedural generation", "game AI",
             "multiplayer networking", "3D animation", "Autodesk Maya"],
    "research": ["research writing", "literature review", "LaTeX", "experimental design", "technical writing",
                 "survey design", "academic presentation", "research methodology", "quantitative analysis",
                 "qualitative analysis", "citation management", "data collection", "research ethics", "peer review",
                 "grant writing", "meta-analysis", "mixed-methods research", "NVivo"],
    "scientific": ["numerical methods", "linear algebra", "SciPy", "mathematical modelling", "differential equations",
                   "Simulink", "Mathematica", "Monte Carlo simulation", "HPC", "MPI", "OpenMP",
                   "mathematical optimization", "operations research", "computational fluid dynamics",
                   "finite difference methods", "COMSOL", "OpenFOAM", "molecular dynamics", "computational chemistry",
                   "Qiskit"],
    "writing": ["documentation", "technical blogging", "copywriting", "public speaking", "presentation design",
                "content writing", "editing", "creative writing", "scriptwriting", "Markdown"],
    "business": ["business analysis", "product management", "market research", "financial modelling",
                 "entrepreneurship", "pitching", "business strategy", "digital marketing", "SEO", "growth marketing",
                 "customer discovery", "startup operations", "requirements gathering", "competitive analysis",
                 "business model canvas", "go-to-market strategy", "pricing strategy", "Lean Startup", "fundraising"],
    "management": ["project management", "Agile", "Scrum", "team leadership", "Jira", "stakeholder management",
                   "risk management", "event management", "time management", "Kanban", "sprint planning",
                   "product roadmapping", "Notion", "Confluence", "mentoring", "conflict resolution"],
    "marketing": ["brand strategy", "content marketing", "email marketing", "Google Analytics",
                  "performance marketing", "market segmentation", "Google Ads", "marketing analytics",
                  "product marketing", "campaign management", "consumer behaviour", "influencer marketing"],
    "communication": ["public relations", "negotiation", "storytelling", "community management",
                      "interpersonal communication", "cross-cultural communication", "crisis communication", "debate",
                      "workshop facilitation", "report writing"],
    "media": ["video editing", "photography", "social media", "podcast production", "Premiere Pro", "After Effects",
              "Canva", "content strategy", "DaVinci Resolve", "Lightroom", "videography", "color grading",
              "audio editing", "sound design"],
    "testing": ["testing", "test automation", "Selenium", "Cypress", "unit testing", "performance testing",
                "manual testing", "API testing", "Postman", "JUnit", "pytest", "integration testing",
                "end-to-end testing", "Playwright", "Jest", "test-driven development", "regression testing", "JMeter",
                "Appium", "code coverage"],
    "web3": ["blockchain", "Solidity", "Web3", "smart contracts", "DeFi", "Ethereum", "Hardhat", "ethers.js", "IPFS",
             "NFTs", "zero-knowledge proofs", "consensus algorithms"],
    "domains": ["fintech", "healthtech", "edtech", "agritech", "GIS", "quantum computing", "bioinformatics",
                "smart grid", "sustainability analytics", "geospatial analysis", "Kaggle competitions", "climate tech",
                "legal tech", "supply chain analytics", "disaster management", "assistive technology", "smart cities",
                "renewable energy", "medical imaging", "remote sensing", "public health",
                "intelligent transportation systems"],
}

# Display labels for the skill catalogue (data/skills.json); every CATEGORIES key needs one.
CATEGORY_LABELS = {
    "programming": "Programming",
    "web": "Web development",
    "mobile": "Mobile development",
    "ai": "AI / ML",
    "data": "Data science",
    "databases": "Databases",
    "cloud": "Cloud",
    "devops": "DevOps",
    "security": "Cybersecurity",
    "networking": "Networking",
    "systems": "Systems & CS fundamentals",
    "embedded": "Embedded systems",
    "iot": "IoT",
    "robotics": "Robotics",
    "electronics": "Electronics",
    "mechanical": "Mechanical & CAD",
    "design": "UI/UX & design",
    "game": "Game & XR",
    "research": "Research",
    "scientific": "Scientific computing",
    "writing": "Writing",
    "business": "Business",
    "management": "Management",
    "marketing": "Marketing",
    "communication": "Communication",
    "media": "Media",
    "testing": "Testing & QA",
    "web3": "Web3",
    "domains": "Domains & competitions",
}

# Internal skill tracks: a primary category, secondary categories and the label stored in the
# record's `role` (used only by the diversity factor; the UI never displays it).
TRACKS = {
    "AI/ML": (["ai"], ["data", "programming", "research", "domains"], 15),
    "Web": (["web"], ["programming", "databases", "design", "devops"], 15),
    "Mobile": (["mobile"], ["programming", "design", "databases"], 7),
    "Systems": (["systems"], ["programming", "devops", "testing"], 9),
    "Data": (["data"], ["databases", "programming", "business", "domains", "scientific"], 9),
    "Design": (["design"], ["media", "writing", "research", "communication"], 6),
    "Embedded": (["embedded"], ["electronics", "iot", "programming"], 5),
    "IoT": (["iot"], ["embedded", "cloud", "networking"], 4),
    "Robotics": (["robotics"], ["embedded", "ai", "mechanical"], 4),
    "Security": (["security"], ["networking", "devops", "programming"], 5),
    "Cloud": (["cloud", "devops"], ["databases", "networking", "programming"], 6),
    "QA": (["testing"], ["devops", "programming", "writing"], 4),
    "Research": (["research"], ["ai", "data", "writing", "domains", "scientific"], 4),
    "Backend": (["web", "databases"], ["programming", "cloud", "devops"], 8),
    "Product": (["business", "management"], ["design", "writing", "data", "marketing", "communication"], 4),
    "Mechanical": (["mechanical"], ["electronics", "robotics", "programming", "scientific"], 3),
    "Game": (["game"], ["programming", "design", "media"], 3),
    "Media": (["media"], ["design", "writing", "business", "marketing", "communication"], 3),
    "Blockchain": (["web3"], ["web", "security", "programming"], 2),
    "Scientific": (["scientific"], ["programming", "research", "data", "mechanical"], 3),
    "Marketing": (["marketing"], ["business", "communication", "media", "data"], 2),
}

PROGRAMS = {
    "AI/ML": ["B.Tech CSE (AI & ML)", "B.Tech CSE", "B.Sc Data Science", "M.Tech CSE"],
    "Web": ["B.Tech CSE", "B.Tech IT", "BCA", "MCA"],
    "Mobile": ["B.Tech CSE", "B.Tech IT", "BCA"],
    "Systems": ["B.Tech CSE", "B.Tech IT", "M.Tech CSE"],
    "Data": ["B.Sc Data Science", "B.Tech CSE", "BCA", "MBA (Business Analytics)"],
    "Design": ["B.Des", "B.Des (Interaction Design)", "BCA"],
    "Embedded": ["B.Tech ECE", "B.Tech EEE"],
    "IoT": ["B.Tech ECE", "B.Tech CSE (IoT)"],
    "Robotics": ["B.Tech Mechatronics", "B.Tech ECE", "B.Tech ME"],
    "Security": ["B.Tech CSE (Cyber Security)", "B.Tech CSE", "MCA"],
    "Cloud": ["B.Tech CSE (Cloud Computing)", "B.Tech IT", "MCA"],
    "QA": ["B.Tech IT", "BCA", "MCA"],
    "Research": ["M.Tech CSE", "M.Sc Statistics", "B.Tech CSE"],
    "Backend": ["B.Tech CSE", "B.Tech IT", "MCA"],
    "Product": ["BBA", "MBA", "B.Tech CSE"],
    "Mechanical": ["B.Tech ME", "B.Tech Aerospace"],
    "Game": ["B.Tech CSE", "B.Des (Game Design)", "BCA"],
    "Media": ["BA Journalism & Mass Communication", "B.Des", "BBA"],
    "Blockchain": ["B.Tech CSE", "B.Tech IT"],
    "Scientific": ["B.Tech CSE", "M.Sc Physics", "B.Tech Chemical", "M.Sc Mathematics"],
    "Marketing": ["BBA", "MBA (Marketing)", "BA Journalism & Mass Communication"],
}
YEARS = ["1st year", "2nd year", "3rd year", "4th year"]

# One-line summaries: skills and experience only. {skills} is the person's strongest skills.
SUMMARIES = {
    "AI/ML": ["AI/ML student focused on {skills}.", "ML builder experienced with {skills}."],
    "Web": ["Full-stack developer experienced with {skills}.", "Web developer building with {skills}."],
    "Mobile": ["Mobile developer shipping apps with {skills}.", "Builds mobile apps with {skills}."],
    "Systems": ["Systems programmer strong in {skills}.", "Low-level developer focused on {skills}."],
    "Data": ["Data analyst comfortable with {skills}.", "Turns raw data into insight using {skills}."],
    "Design": ["Product designer skilled in {skills}.", "UI/UX designer working across {skills}."],
    "Embedded": ["Embedded developer working with {skills}.", "Hardware tinkerer experienced with {skills}."],
    "IoT": ["IoT builder connecting devices with {skills}.", "Connected-device developer using {skills}."],
    "Robotics": ["Robotics developer experienced with {skills}.", "Builds autonomous systems with {skills}."],
    "Security": ["Security enthusiast experienced with {skills}.", "Cybersecurity student focused on {skills}."],
    "Cloud": ["Cloud and DevOps engineer working with {skills}.", "Deploys and automates systems with {skills}."],
    "QA": ["Quality-focused engineer experienced in {skills}.", "Keeps software reliable with {skills}."],
    "Research": ["Research-minded student skilled in {skills}.", "Runs and writes up studies using {skills}."],
    "Backend": ["Backend developer focused on {skills}.", "Server-side developer working with {skills}."],
    "Product": ["Product-minded builder skilled in {skills}.", "Turns ideas into plans with {skills}."],
    "Mechanical": ["Mechanical designer experienced with {skills}.", "Designs and prototypes parts with {skills}."],
    "Game": ["Game developer building with {skills}.", "Creates interactive worlds with {skills}."],
    "Media": ["Content creator skilled in {skills}.", "Tells stories through {skills}."],
    "Blockchain": ["Web3 developer experienced with {skills}.", "Builds decentralised apps with {skills}."],
    "Scientific": ["Scientific-computing student skilled in {skills}.", "Models and simulates systems with {skills}."],
    "Marketing": ["Marketer experienced with {skills}.", "Grows audiences and brands using {skills}."],
}
LEARNER_SUMMARIES = ["Early-stage learner picking up {skills}.", "Beginner building first projects with {skills}."]
DEVELOPING_SUMMARIES = ["Growing hands-on skills in {skills}.", "Building project experience with {skills}.",
                        "Developing skills in {skills} through course projects."]
EXPERT_SUMMARIES = ["Experienced {area} developer, strongest in {skills}.", "Seasoned in {skills}, mentors teammates."]

FIRST_NAMES = [
    "Aarav", "Aditi", "Aditya", "Ananya", "Arjun", "Avni", "Ayaan", "Bhavya", "Chirag", "Diya", "Devansh", "Divya",
    "Eshan", "Gauri", "Harsh", "Ishita", "Ishaan", "Jahnavi", "Kabir", "Kavya", "Krish", "Lavanya", "Manav", "Meera",
    "Mihir", "Naina", "Nikhil", "Nisha", "Om", "Pari", "Pranav", "Priya", "Rahul", "Riya", "Rohan", "Saanvi", "Sahil",
    "Sakshi", "Samar", "Sanya", "Shaurya", "Shreya", "Siddharth", "Simran", "Tanvi", "Tanish", "Utkarsh", "Vaishnavi",
    "Vedant", "Vihaan", "Yash", "Zoya", "Akash", "Anjali", "Ansh", "Aryan", "Deepika", "Gaurav", "Himani", "Jatin",
    "Karan", "Khushi", "Kunal", "Mansi", "Mayank", "Neha", "Palak", "Pooja", "Prateek", "Rachit", "Ritika", "Sneha",
    "Tushar", "Varun", "Vidhi", "Ayesha", "Farhan", "Imran", "Sara", "Zaid", "Gurpreet", "Harleen", "Jaspreet",
    "Manpreet", "Navjot", "Anmol", "Arnav", "Dhruv", "Isha", "Kriti", "Lakshya", "Mehak", "Nandini", "Parth",
    "Radhika", "Shivam", "Tara", "Uday", "Vanshika", "Yuvraj", "Aisha", "Daniel", "Joel", "Maria", "Nathan",
    "Rebecca", "Tenzin", "Pema", "Sonam", "Dechen", "Abhinav", "Akshita", "Amrita", "Bhuvan", "Charu", "Darsh",
    "Ekta", "Garima", "Hrithik", "Ira", "Jayesh", "Juhi", "Keshav", "Lakshmi", "Madhav", "Nitya", "Ojas", "Pallavi",
    "Raghav", "Sameer", "Shruti", "Sujal", "Swati", "Tejas", "Unnati", "Vivek", "Yamini", "Aniket", "Bhoomi",
    "Chetan", "Disha", "Faiz", "Gayatri", "Hemant", "Indira", "Jyoti", "Kartik", "Lata", "Mridul", "Naveen",
    "Pankaj", "Rashmi", "Snehal", "Tanmay", "Vaibhav", "Zainab", "Rehan", "Sana", "Arpit", "Muskan",
]
LAST_NAMES = [
    "Sharma", "Verma", "Gupta", "Singh", "Rawat", "Negi", "Bisht", "Joshi", "Pant", "Bhatt", "Chauhan", "Thapa",
    "Rana", "Kumar", "Mehta", "Shah", "Patel", "Desai", "Iyer", "Nair", "Menon", "Reddy", "Rao", "Naidu", "Pillai",
    "Das", "Bose", "Ghosh", "Banerjee", "Mukherjee", "Kapoor", "Malhotra", "Khanna", "Arora", "Sethi", "Bhatia",
    "Agarwal", "Jain", "Mishra", "Tiwari", "Pandey", "Dubey", "Saxena", "Srivastava", "Yadav", "Chaudhary", "Kaur",
    "Gill", "Sandhu", "Dhillon", "Khan", "Ahmed", "Siddiqui", "Qureshi", "Mirza", "Fernandes", "D'Souza", "Thomas",
    "George", "Kurian", "Bhandari", "Dhami", "Kandari", "Semwal", "Nautiyal", "Uniyal", "Dobhal", "Butola",
    "Rautela", "Mehra", "Lepcha", "Bhutia", "Tamang", "Gurung", "Sherpa", "Kulkarni", "Deshpande", "Patil",
    "Jadhav", "Shinde", "Hegde", "Shetty", "Kamath", "Bhat", "Krishnan", "Subramanian", "Venkatesh", "Chopra",
    "Ahuja", "Grover", "Talwar", "Bajaj", "Goel", "Mittal", "Bansal", "Singhal", "Sinha", "Prasad", "Thakur",
    "Chandra", "Bora", "Baruah", "Saikia", "Hazarika", "Mondal", "Sen", "Dutta", "Roy",
]

# The demo participant: Anubhav Bisht's record, under a recognisable id (it was tf-002).
DEMO_PARTICIPANT = ("tf-002", "TF-P001")
# Interest requests: how many participants express interest in each project, and how far the
# host's review has got (interested, under review, accepted, declined).
INTERESTS_PER_PROJECT = (8, 12)
STATUS_WEIGHTS = {"interested": 45, "under_review": 25, "accepted": 20, "declined": 10}
GENERAL = ["Git", "documentation", "public speaking", "project management", "Agile", "team leadership",
           "technical writing", "presentation design", "time management"]

# The original sample records predate summaries and programmes; theirs are written by hand.
# The four TeamForge members' lines follow their Phase-I project areas.
KEPT = {
    "tf-001": ("C++ developer focused on integration, system design and Git workflows.", "B.Tech CSE · 2nd year"),
    "tf-002": ("Qt and QML developer who also owns UI design, testing and documentation.", "B.Tech CSE · 2nd year"),
    "tf-003": ("C++ and OOP specialist with a strong grip on design patterns.", "B.Tech CSE · 2nd year"),
    "tf-004": ("DSA and algorithms specialist working in C++ and Python.", "B.Tech CSE · 2nd year"),
    "tf-101": ("Machine-learning student with strong Python and data-analysis skills.", "B.Tech CSE (AI & ML) · 3rd year"),
    "tf-102": ("Front-end developer building with JavaScript and React.", "B.Tech IT · 2nd year"),
    "tf-103": ("Testing-focused engineer who documents thoroughly and writes Python.", "BCA · 3rd year"),
    "tf-104": ("Java backend developer working with SQL databases.", "B.Tech CSE · 4th year"),
    "tf-105": ("Competitive programmer with solid DSA and C++.", "B.Tech CSE · 2nd year"),
    "tf-106": ("Embedded developer using Arduino, embedded C and electronics.", "B.Tech ECE · 3rd year"),
    "tf-107": ("C++ and Qt developer comfortable with QML and OOP.", "B.Tech CSE · 3rd year"),
    "tf-108": ("UI designer who prototypes in Figma.", "B.Des · 2nd year"),
}

DISPLAY = {skill.lower(): skill for skills in CATEGORIES.values() for skill in skills}


def key(skill):
    return skill.lower()


def prose_list(skills):
    names = [DISPLAY.get(s, s) for s in skills]
    return names[0] if len(names) == 1 else ", ".join(names[:-1]) + " and " + names[-1]


def popularity(skill):
    # Categories list common skills first; earlier entries are picked more often, so core
    # skills (Python, React, SQL ...) are widespread and niche ones rarer, as in a real cohort.
    for skills in CATEGORIES.values():
        if skill in skills:
            return 1.0 / (1.0 + 0.3 * skills.index(skill))
    return 0.2


def weighted_sample(rng, pool, k):
    pool = list(pool)
    chosen = []
    for _ in range(min(k, len(pool))):
        pick = rng.choices(pool, weights=[popularity(s) for s in pool])[0]
        chosen.append(pick)
        pool.remove(pick)
    return chosen


def pick_level(rng, base, boost=0):
    return max(1, min(5, base + boost + rng.choice([-1, 0, 0, 0, 1])))


def profile(rng, student_id, name):
    track = rng.choices(list(TRACKS), weights=[t[2] for t in TRACKS.values()])[0]
    primary, secondary, _ = TRACKS[track]
    experience = rng.choices([1, 2, 3, 4, 5], weights=[11, 24, 34, 22, 9])[0]

    skills = []
    pool = [s for c in primary for s in CATEGORIES[c]]
    skills += weighted_sample(rng, pool, rng.randint(2, 4))
    for category in rng.sample(secondary, rng.randint(1, 2)):
        skills += weighted_sample(rng, CATEGORIES[category], rng.randint(1, 2))
    if rng.random() < 0.2:
        skills.append(rng.choice(GENERAL))

    offered = {}
    for index, skill in enumerate(dict.fromkeys(key(s) for s in skills)):
        offered[skill] = pick_level(rng, experience, 1 if index == 0 else 0 if index < 3 else -1)
        if len(offered) == 8:
            break

    others = [c for c in CATEGORIES if c not in primary]
    wanted = {key(s) for c in rng.sample(others, 2) for s in rng.sample(CATEGORIES[c], 1)} - set(offered)
    wanted |= {key(rng.choice(pool))} - set(offered)

    program = rng.choice(PROGRAMS[track])
    years = YEARS[:2] if program.startswith(("M.", "MCA", "MBA")) else YEARS
    return {
        "id": student_id,
        "name": name,
        "role": track,
        "summary": summary(rng, track, experience, offered, {key(s) for s in pool}),
        "program": f"{program} · {rng.choice(years)}",
        "skillsOffered": offered,
        "skillsWanted": sorted(wanted)[:3],
    }


def summary(rng, track, experience, offered, primary):
    # The track's own skills lead, so the sentence reads as one coherent specialism.
    ranked = sorted(offered, key=lambda s: (s not in primary, -offered[s]))
    top = ranked[:3] if len([s for s in ranked if s in primary]) >= 2 else ranked[:2]
    # The wording follows the actual levels, so a beginner is never "experienced".
    average = sum(offered.values()) / len(offered)
    if experience <= 1 or average < 2.0:
        template = rng.choice(LEARNER_SUMMARIES)
    elif average < 2.75:
        template = rng.choice(DEVELOPING_SUMMARIES)
    elif experience >= 5 and rng.random() < 0.5 and track != "Marketing":
        template = rng.choice(EXPERT_SUMMARIES)
    else:
        template = rng.choice(SUMMARIES[track])
    area = {"AI/ML": "ML", "Web": "web", "Mobile": "mobile", "Data": "data",
            "Scientific": "scientific-computing"}.get(track, track.lower())
    return template.format(skills=prose_list(top), area=area)


def ensure_coverage(rng, students):
    """Every skill in the universe is offered by at least one generated participant.

    A missing skill goes to a generated participant whose track uses its category, preferring
    one with room (fewer than 8 skills). If none has room, that participant's weakest skill is
    dropped, but never one they alone offer or one named in their summary.
    """
    holders = Counter(s for st in students for s in st["skillsOffered"])
    generated = [s for s in students if s["id"].startswith("tf-1") and len(s["id"]) == 7]

    def droppable(student):
        return [s for s in reversed(list(student["skillsOffered"]))
                if holders[s] > 1 and DISPLAY.get(s, s) not in student["summary"]]

    for category, skills in CATEGORIES.items():
        related = [s for s in generated if category in TRACKS[s["role"]][0] + TRACKS[s["role"]][1]] or generated
        for skill in skills:
            if holders[key(skill)]:
                continue
            room = [s for s in related if len(s["skillsOffered"]) < 8]
            student = rng.choice(room or [s for s in related if droppable(s)])
            if len(student["skillsOffered"]) >= 8:
                victim = droppable(student)[0]
                del student["skillsOffered"][victim]
                holders[victim] -= 1
            student["skillsOffered"][key(skill)] = rng.randint(2, 4)
            holders[key(skill)] += 1


def build_students(rng):
    existing = json.loads((DATA / "students.json").read_text(encoding="utf-8"))
    kept = []
    for record in existing["students"][:12]:
        original_id = DEMO_PARTICIPANT[0] if record["id"] == DEMO_PARTICIPANT[1] else record["id"]
        # Availability is no longer part of TeamForge; older files carried it.
        record = {k: v for k, v in record.items() if k not in ("summary", "program", "availability")}
        if record["id"] == DEMO_PARTICIPANT[0]:
            record["id"] = DEMO_PARTICIPANT[1]
        items = list(record.items())
        items[3:3] = [("summary", KEPT[original_id][0]), ("program", KEPT[original_id][1])]
        kept.append(dict(items))
    used = {s["name"] for s in kept}
    students = list(kept)
    number = 1001
    while len(students) < TOTAL_STUDENTS:
        name = f"{rng.choice(FIRST_NAMES)} {rng.choice(LAST_NAMES)}"
        if name in used:
            continue
        used.add(name)
        students.append(profile(rng, f"tf-{number}", name))
        number += 1
    ensure_coverage(rng, students)
    return {
        "version": 1,
        "note": ("Local sample data. The four TeamForge members' skills follow their Phase-I roles, but "
                 "levels (1-5) are illustrative placeholders, not assessments. TF-P001 (Anubhav Bisht) is "
                 "the demo participant. All other participants are generated profiles, not real students. "
                 "Regenerate with tools/generate_sample_data.py."),
        "students": students,
    }


def build_requirements():
    def project(rid, name, summary, kind, skills, size):
        return {"id": rid, "name": name, "type": kind, "summary": summary,
                "requiredSkills": {key(s): level for s, level in skills.items()},
                "minTeamSize": size[0], "maxTeamSize": size[1]}

    return {
        "version": 1,
        "note": "Local sample projects.",
        "requirements": [
            project("req-001", "CampusConnect", "Smart Campus Collaboration Challenge", "Hackathon",
                    {"C++": 3, "Qt": 3, "QML": 2, "DSA": 3, "testing": 3, "UI design": 3},
                    (3, 4)),
            project("req-002", "CivicLens", "Data for Smarter Communities", "PBL / Course Project",
                    {"Python": 3, "data analysis": 3, "databases": 3, "SQL": 3, "UI design": 2},
                    (2, 4)),
            project("req-003", "AgriVision", "AI Crop Health Challenge", "AI/ML Project",
                    {"Python": 3, "machine learning": 3, "computer vision": 3, "deep learning": 2, "OpenCV": 2},
                    (3, 5)),
            project("req-004", "AlumniLoop", "Student–Alumni Mentorship Platform", "Web / Product Project",
                    {"JavaScript": 3, "React": 3, "Node.js": 3, "PostgreSQL": 3, "UI design": 3, "testing": 2},
                    (4, 6)),
            project("req-005", "LangBridge", "Low-Resource Language Research Challenge", "Research Project",
                    {"Python": 4, "NLP": 3, "statistics": 3, "research writing": 3, "machine learning": 3},
                    (2, 3)),
            project("req-006", "GreenGrid", "Campus Energy Intelligence", "Technology Competition",
                    {"embedded C": 3, "IoT": 3, "electronics": 3, "Python": 2, "time series forecasting": 2,
                     "cloud": 2},
                    (3, 4)),
            project("req-007", "SecureStack", "Applied Cybersecurity Challenge", "Hackathon",
                    {"cybersecurity": 3, "network security": 3, "penetration testing": 2, "Linux": 3, "Python": 2},
                    (3, 5)),
            project("req-008", "HealthRoute", "Accessible Healthcare Navigation", "PBL / Course Project",
                    {"Flutter": 3, "Firebase": 2, "accessibility design": 2, "UI design": 3, "GIS": 2, "testing": 2},
                    (2, 4)),
        ],
    }


def build_interests(rng, students, requirements):
    """Participants who expressed interest in each project, at different review stages.

    Interest goes mostly to people whose skills fit (they cover at least one required skill),
    as it would in practice. The demo participant has a request under review and one open, so
    the participant and host views both have something to show from the first run.
    """
    requests = []

    def add(student_id, requirement_id, status):
        requests.append({"id": f"int-{len(requests) + 1:04d}", "studentId": student_id,
                         "requirementId": requirement_id, "status": status})

    add(DEMO_PARTICIPANT[1], "req-001", "under_review")
    add(DEMO_PARTICIPANT[1], "req-004", "interested")
    members = {"tf-001", DEMO_PARTICIPANT[1], "tf-003", "tf-004"}
    for requirement in requirements["requirements"]:
        needed = requirement["requiredSkills"]
        fits = [s for s in students["students"] if s["id"] not in members
                and any(s["skillsOffered"].get(skill, 0) >= level for skill, level in needed.items())]
        fits.sort(key=lambda s: (-sum(1 for k, v in needed.items() if s["skillsOffered"].get(k, 0) >= v), s["id"]))
        pool = fits[:40]
        for student in rng.sample(pool, min(len(pool), rng.randint(*INTERESTS_PER_PROJECT))):
            status = rng.choices(list(STATUS_WEIGHTS), weights=list(STATUS_WEIGHTS.values()))[0]
            add(student["id"], requirement["id"], status)
    return {"version": 1, "note": "Local sample interest requests.", "requests": requests}


def build_catalogue():
    category_of = {}
    for category, skills in CATEGORIES.items():
        for skill in skills:
            category_of.setdefault(key(skill), CATEGORY_LABELS[category])  # first category wins
    return {
        "version": 1,
        "note": ("Skill catalogue: the category of each known skill. Skills stay open-ended; any new skill "
                 "is valid without an entry here."),
        "skills": [{"name": name, "category": category_of[name]} for name in sorted(category_of)],
    }


def write(path, payload):
    path.write_text(json.dumps(payload, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    assert set(CATEGORY_LABELS) == set(CATEGORIES), "every category needs exactly one display label"
    all_keys = [key(s) for skills in CATEGORIES.values() for s in skills]
    duplicates = sorted(s for s, n in Counter(all_keys).items() if n > 1)
    assert not duplicates, f"skills listed more than once: {duplicates}"

    rng = random.Random(SEED)
    students = build_students(rng)
    requirements = build_requirements()
    catalogue = build_catalogue()
    interests = build_interests(rng, students, requirements)
    distinct = {s for st in students["students"] for s in st["skillsOffered"]}
    distinct |= {s for r in requirements["requirements"] for s in r["requiredSkills"]}
    assert len(distinct) >= MIN_DISTINCT_SKILLS, f"only {len(distinct)} distinct skills"
    offered = {s for st in students["students"] for s in st["skillsOffered"]}
    uncovered = sorted(set(all_keys) - offered)
    assert not uncovered, f"catalogue skills nobody offers: {uncovered}"
    write(DATA / "students.json", students)
    write(DATA / "requirements.json", requirements)
    write(DATA / "teams.json", {"version": 1, "teams": []})
    write(DATA / "skills.json", catalogue)
    write(DATA / "interest_requests.json", interests)
    print(f"{len(students['students'])} participants, {len(requirements['requirements'])} projects, "
          f"{len(distinct)} distinct skills, {len(catalogue['skills'])} catalogue skills, "
          f"{len(interests['requests'])} interest requests")
