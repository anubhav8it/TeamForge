# Resume fixtures

Fictional people (example.com addresses, placeholder phone numbers), used by `tst_resume`,
`tst_controller` and `tst_ui`. Not real students.

| File | Made with |
|---|---|
| `priya-nair.pdf` | `priya-nair.html` printed by Microsoft Edge (headless, `--print-to-pdf --no-pdf-header-footer`): the same PDF engine as Chrome's "Save as PDF", with subset CID fonts, ToUnicode maps and compressed streams |
| `photo-sharp.jpg` | `priya-nair.html` screenshotted by headless Edge (950 × 1250), saved as a greyscale JPEG: a sharp "photo" of a resume |
| `photo-blurry.jpg` | the same image with a Gaussian blur of sigma 2 px: refused as too blurry before it is sent |
| `rahul-verma.docx` | `python -I tools/make_resume_docx.py tests/data/resumes/rahul-verma.docx`: WordprocessingML with a header part, a text box (written twice by Word, in `mc:Choice` and `mc:Fallback`), tabs and bullets |

The tests assert what each file says, so regenerate the expectations if you change a fixture.
