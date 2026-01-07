#!/usr/bin/env python3
"""
PDF to Text Converter using PyPDF2
Converts TP_CM_Trimmed_NNF_PROTOCOL_6.3.pdf to text format
"""

import PyPDF2
import os

def pdf_to_text(pdf_path, output_path=None):
    """
    Convert PDF file to text using PyPDF2
    
    Args:
        pdf_path (str): Path to the PDF file
        output_path (str, optional): Path to save the text file. 
                                   If None, creates a .txt file with same name as PDF
    
    Returns:
        str: Extracted text content
    """
    
    # Check if PDF file exists
    if not os.path.exists(pdf_path):
        print(f"Error: PDF file '{pdf_path}' not found!")
        return None
    
    # Generate output path if not provided
    if output_path is None:
        output_path = os.path.splitext(pdf_path)[0] + '.txt'
    
    extracted_text = ""
    
    try:
        # Open PDF file in binary mode
        with open(pdf_path, 'rb') as pdf_file:
            # Create PDF reader object
            pdf_reader = PyPDF2.PdfReader(pdf_file)
            
            # Get number of pages
            num_pages = len(pdf_reader.pages)
            print(f"Processing PDF with {num_pages} pages...")
            
            # Extract text from each page
            for page_num in range(num_pages):
                page = pdf_reader.pages[page_num]
                page_text = page.extract_text()
                
                # Add page separator and page number
                extracted_text += f"\n{'='*50}\n"
                extracted_text += f"PAGE {page_num + 1}\n"
                extracted_text += f"{'='*50}\n"
                extracted_text += page_text + "\n"
                
                print(f"Processed page {page_num + 1}/{num_pages}")
        
        # Save extracted text to file
        with open(output_path, 'w', encoding='utf-8') as text_file:
            text_file.write(extracted_text)
        
        print(f"\nSuccess! Text extracted and saved to: {output_path}")
        print(f"Total characters extracted: {len(extracted_text)}")
        
        return extracted_text
        
    except Exception as e:
        print(f"Error processing PDF: {str(e)}")
        return None

def main():
    """Main function to run the PDF to text conversion"""
    
    # Get the directory where this script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Define paths
    pdf_filename = "TP_CM_Trimmed_NNF_PROTOCOL_6.3.pdf"
    pdf_path = os.path.join(script_dir, pdf_filename)
    output_path = os.path.join(script_dir, "TP_CM_Trimmed_NNF_PROTOCOL_6.3.txt")
    
    print("PDF to Text Converter")
    print("=" * 40)
    print(f"Input PDF: {pdf_path}")
    print(f"Output Text: {output_path}")
    print("=" * 40)
    
    # Convert PDF to text
    result = pdf_to_text(pdf_path, output_path)
    
    if result:
        print("\nConversion completed successfully!")
        
        # Show first 500 characters as preview
        print("\nPreview of extracted text:")
        print("-" * 30)
        print(result[:500] + "..." if len(result) > 500 else result)
    else:
        print("\nConversion failed!")

if __name__ == "__main__":
    main()
