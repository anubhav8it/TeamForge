"""Writes the DOCX resume fixture (tests/data/resumes/rahul-verma.docx), a fictional person, with real
WordprocessingML structure. Usage: python -I tools/make_resume_docx.py <out.docx>"""
import sys
import zipfile
from xml.sax.saxutils import escape

W = 'xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" ' \
    'xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" ' \
    'xmlns:wps="http://schemas.microsoft.com/office/word/2010/wordprocessingShape"'


def run(text, bold=False):
    props = '<w:rPr><w:b/></w:rPr>' if bold else ''
    return f'<w:r>{props}<w:t xml:space="preserve">{escape(text)}</w:t></w:r>'


def para(*runs, style=None):
    ppr = f'<w:pPr><w:pStyle w:val="{style}"/></w:pPr>' if style else ''
    return f'<w:p>{ppr}{"".join(runs)}</w:p>'


body = [
    # A text box: Word writes it twice (mc:Choice and the legacy mc:Fallback copy).
    '<w:p><w:r><mc:AlternateContent><mc:Choice Requires="wps"><w:drawing><wps:txbx><w:txbxContent>'
    + para(run('Rahul Verma', True)) +
    '</w:txbxContent></wps:txbx></w:drawing></mc:Choice><mc:Fallback><w:pict><w:txbxContent>'
    + para(run('Rahul Verma', True)) +
    '</w:txbxContent></w:pict></mc:Fallback></mc:AlternateContent></w:r></w:p>',
    para(run('Data Analyst'), '<w:r><w:tab/></w:r>', run('rahul@example.com')),
    para(run('Age: 22')),
    para(run('SKILLS', True), style='Heading1'),
    para(run('Python, SQL, Power BI, Tableau, Excel, Pandas, Machine Learning, Statistics')),
    para(run('2 years of experience with SQL and Excel')),
    para(run('EXPERIENCE', True), style='Heading1'),
    para(run('Data Analyst Intern', True), run(' | Insight Co | '), run('June 2022 - May 2023')),
    para(run('• Built Tableau dashboards on sales data using SQL and Python.')),
    para(run('Analyst', True), run(' | Numbers Ltd | '), run('Jun 2023 – Present')),
    para(run('• Automated Excel reports with Python and Pandas.')),
    para(run('PROJECTS', True), style='Heading1'),
    para(run('Retail demand forecasting with Python & scikit-learn')),
    para(run('COVID-19 data dashboard in Power BI')),
    para(run('EDUCATION', True), style='Heading1'),
    para(run('BCA, Doon University – 2019 to 2022')),
    para(run('CERTIFICATIONS', True), style='Heading1'),
    para(run('Google Data Analytics Professional Certificate')),
    para(run('AWARDS', True), style='Heading1'),
    para(run('Best Project Award, Doon University Tech Fest 2021 & more')),
]
document = f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:document {W}><w:body>{"".join(body)}<w:sectPr/></w:body></w:document>'
header = f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:hdr {W}>{para(run("Curriculum Vitae"))}</w:hdr>'
content_types = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
                 '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                 '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
                 '<Default Extension="xml" ContentType="application/xml"/>'
                 '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
                 '</Types>')
rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
        '</Relationships>')
with zipfile.ZipFile(sys.argv[1], 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('[Content_Types].xml', content_types)
    z.writestr('_rels/.rels', rels)
    z.writestr('word/document.xml', document)
    z.writestr('word/header1.xml', header)
print('wrote', sys.argv[1])
