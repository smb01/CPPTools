// bin2hex.cpp : Defines the entry point for the console application.
//
#include <Windows.h>
#include <stdio.h>
#include <string.h>


BOOL ClearMZ(const TCHAR* lpszFilename)
{
	BOOL	bRet = FALSE;
	CHAR	szDosHeader[] = { 0x00, 0x00 };
	DWORD	dwWritten = 0;
	HANDLE	hFile = NULL;
	hFile = CreateFile(lpszFilename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return bRet;
	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	bRet = WriteFile(hFile, szDosHeader, 2, &dwWritten, NULL);
	CloseHandle(hFile);
	return bRet;
}

int main( int argc, char* argv[] )
{
	FILE* fin = NULL; 
	FILE* fout = NULL;
	char* nameVar = "data";
	int c = 0;
	int n = 0;

	if(argc < 3)
	{
		printf("bin2hex.exe 'infile' 'outfile' 'name' [-ClearMZ]");
		return 0;
	}

	if (argc > 3)
		nameVar = argv[3];
	if (argc > 4 && stricmp(argv[4], "-ClearMZ") == 0)
		ClearMZ(argv[1]);

	fin	= fopen(argv[1], "rb");
	if(fin == NULL) 
	{
		printf("not open file %s", argv[1]);
		return 0;
	}

	fout = fopen(argv[2], "w");
	if(fout == NULL) 
	{
		printf("not create file %s", argv[2]);
		return 0;
	}

	fprintf(fout, "unsigned char %s[] =\n{", nameVar);

	while((c = fgetc(fin)) >= 0) 
	{
		if(n > 0)
			fprintf(fout, ", ");
		if((n % 16) == 0) 
			fprintf(fout, "\n\t");
		fprintf(fout, "0x%02x", c);
		n++;
	}

	fprintf(fout, "\n};\n" );
	fclose(fout);
	fclose(fin);

	return 1;
}
