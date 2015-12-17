/*
 * This file produced from types.pl
 */

/**
 * Return the number of bytes needed by specified type.
 */
int
tds_get_size_by_type(int servertype)
{
	switch (servertype) {
	case SYBVOID:
		return 0;
	case SYBBIT:
	case SYBBITN:
	case SYBINT1:
	case SYBSINT1:
	case SYBUINT1:
		return 1;
	case SYBINT2:
	case SYBUINT2:
		return 2;
	case SYBMSDATE:
		return 3;
	case SYBDATE:
	case SYBDATEN:
	case SYBDATETIME4:
	case SYBINT4:
	case SYBMONEY4:
	case SYBREAL:
	case SYBTIME:
	case SYBTIMEN:
	case SYBUINT4:
		return 4;
	case SYB5INT8:
	case SYBDATETIME:
	case SYBFLT8:
	case SYBINT8:
	case SYBINTERVAL:
	case SYBMONEY:
	case SYBUINT8:
		return 8;
	case SYBUNIQUE:
		return 16;
	default:
		return -1;
	}
}

/**
 * tds_get_varint_size() returns the size of a variable length integer
 * returned in a TDS 7.0 result string
 */
int
tds_get_varint_size(TDSCONNECTION * conn, int datatype)
{
	switch (datatype) {
	case SYBBIT:
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBFLT8:
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBMONEY:
	case SYBMONEY4:
	case SYBREAL:
	case SYBVOID:
		return 0;
	case SYBIMAGE:
	case SYBTEXT:
		return 4;
	}

	if (IS_TDS7_PLUS(conn)) {
		switch (datatype) {
		case SYBINT8:
			return 0;
		case XSYBBINARY:
		case XSYBCHAR:
		case XSYBNCHAR:
		case XSYBNVARCHAR:
		case XSYBVARBINARY:
		case XSYBVARCHAR:
			return 2;
		case SYBNTEXT:
		case SYBVARIANT:
			return 4;
		case SYBMSUDT:
		case SYBMSXML:
			return 8;
		}
	} else if (IS_TDS50(conn)) {
		switch (datatype) {
		case SYB5INT8:
		case SYBDATE:
		case SYBINTERVAL:
		case SYBSINT1:
		case SYBTIME:
		case SYBUINT1:
		case SYBUINT2:
		case SYBUINT4:
		case SYBUINT8:
			return 0;
		case SYBUNITEXT:
		case SYBXML:
			return 4;
		case SYBLONGBINARY:
		case SYBLONGCHAR:
			return 5;
		}
	}
	return 1;
}

/**
 * Return type suitable for conversions (convert all nullable types to fixed type)
 * @param srctype type to convert
 * @param colsize size of type
 * @result type for conversion
 */
int
tds_get_conversion_type(int srctype, int colsize)
{
	switch (srctype) {
	case SYBBITN:
		return SYBBIT;
	case SYBDATEN:
		return SYBDATE;
	case SYBDATETIMN:
		switch (colsize) {
		case 8:
			return SYBDATETIME;
		case 4:
			return SYBDATETIME4;
		}
		break;
	case SYBFLTN:
		switch (colsize) {
		case 8:
			return SYBFLT8;
		case 4:
			return SYBREAL;
		}
		break;
	case SYBINTN:
		switch (colsize) {
		case 8:
			return SYBINT8;
		case 4:
			return SYBINT4;
		case 2:
			return SYBINT2;
		case 1:
			return SYBINT1;
		}
		break;
	case SYBMONEYN:
		switch (colsize) {
		case 8:
			return SYBMONEY;
		case 4:
			return SYBMONEY4;
		}
		break;
	case SYBTIMEN:
		return SYBTIME;
	case SYBUINTN:
		switch (colsize) {
		case 8:
			return SYBUINT8;
		case 4:
			return SYBUINT4;
		case 2:
			return SYBUINT2;
		case 1:
			return SYBUINT1;
		}
		break;
	case SYB5INT8:
		return SYBINT8;
	}
	return srctype;
}

const unsigned char tds_type_flags_ms[256] = {
	/*   0 empty                */	TDS_TYPEFLAG_INVALID,
	/*   1 empty                */	TDS_TYPEFLAG_INVALID,
	/*   2 empty                */	TDS_TYPEFLAG_INVALID,
	/*   3 empty                */	TDS_TYPEFLAG_INVALID,
	/*   4 empty                */	TDS_TYPEFLAG_INVALID,
	/*   5 empty                */	TDS_TYPEFLAG_INVALID,
	/*   6 empty                */	TDS_TYPEFLAG_INVALID,
	/*   7 empty                */	TDS_TYPEFLAG_INVALID,
	/*   8 empty                */	TDS_TYPEFLAG_INVALID,
	/*   9 empty                */	TDS_TYPEFLAG_INVALID,
	/*  10 empty                */	TDS_TYPEFLAG_INVALID,
	/*  11 empty                */	TDS_TYPEFLAG_INVALID,
	/*  12 empty                */	TDS_TYPEFLAG_INVALID,
	/*  13 empty                */	TDS_TYPEFLAG_INVALID,
	/*  14 empty                */	TDS_TYPEFLAG_INVALID,
	/*  15 empty                */	TDS_TYPEFLAG_INVALID,
	/*  16 empty                */	TDS_TYPEFLAG_INVALID,
	/*  17 empty                */	TDS_TYPEFLAG_INVALID,
	/*  18 empty                */	TDS_TYPEFLAG_INVALID,
	/*  19 empty                */	TDS_TYPEFLAG_INVALID,
	/*  20 empty                */	TDS_TYPEFLAG_INVALID,
	/*  21 empty                */	TDS_TYPEFLAG_INVALID,
	/*  22 empty                */	TDS_TYPEFLAG_INVALID,
	/*  23 empty                */	TDS_TYPEFLAG_INVALID,
	/*  24 empty                */	TDS_TYPEFLAG_INVALID,
	/*  25 empty                */	TDS_TYPEFLAG_INVALID,
	/*  26 empty                */	TDS_TYPEFLAG_INVALID,
	/*  27 empty                */	TDS_TYPEFLAG_INVALID,
	/*  28 empty                */	TDS_TYPEFLAG_INVALID,
	/*  29 empty                */	TDS_TYPEFLAG_INVALID,
	/*  30 empty                */	TDS_TYPEFLAG_INVALID,
	/*  31 SYBVOID              */	TDS_TYPEFLAG_FIXED,
	/*  32 empty                */	TDS_TYPEFLAG_INVALID,
	/*  33 empty                */	TDS_TYPEFLAG_INVALID,
	/*  34 SYBIMAGE             */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/*  35 SYBTEXT              */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_COLLATE|TDS_TYPEFLAG_ASCII,
	/*  36 SYBUNIQUE            */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_FIXED,
	/*  37 SYBVARBINARY         */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/*  38 SYBINTN              */	TDS_TYPEFLAG_NULLABLE,
	/*  39 SYBVARCHAR           */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_ASCII,
	/*  40 SYBMSDATE            */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_FIXED,
	/*  41 SYBMSTIME            */	TDS_TYPEFLAG_NULLABLE,
	/*  42 SYBMSDATETIME2       */	TDS_TYPEFLAG_NULLABLE,
	/*  43 SYBMSDATETIMEOFFSET  */	TDS_TYPEFLAG_NULLABLE,
	/*  44 empty                */	TDS_TYPEFLAG_INVALID,
	/*  45 SYBBINARY            */	TDS_TYPEFLAG_VARIABLE,
	/*  46 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/*  47 SYBCHAR              */	TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_ASCII,
	/*  48 SYBINT1              */	TDS_TYPEFLAG_FIXED,
	/*  49 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/*  50 SYBBIT               */	TDS_TYPEFLAG_FIXED,
	/*  51 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/*  52 SYBINT2              */	TDS_TYPEFLAG_FIXED,
	/*  53 empty                */	TDS_TYPEFLAG_INVALID,
	/*  54 empty                */	TDS_TYPEFLAG_INVALID,
	/*  55 empty                */	TDS_TYPEFLAG_INVALID,
	/*  56 SYBINT4              */	TDS_TYPEFLAG_FIXED,
	/*  57 empty                */	TDS_TYPEFLAG_INVALID,
	/*  58 SYBDATETIME4         */	TDS_TYPEFLAG_FIXED,
	/*  59 SYBREAL              */	TDS_TYPEFLAG_FIXED,
	/*  60 SYBMONEY             */	TDS_TYPEFLAG_FIXED,
	/*  61 SYBDATETIME          */	TDS_TYPEFLAG_FIXED,
	/*  62 SYBFLT8              */	TDS_TYPEFLAG_FIXED,
	/*  63 empty                */	TDS_TYPEFLAG_INVALID,
	/*  64 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/*  65 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/*  66 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/*  67 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/*  68 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE,
	/*  69 empty                */	TDS_TYPEFLAG_INVALID,
	/*  70 empty                */	TDS_TYPEFLAG_INVALID,
	/*  71 empty                */	TDS_TYPEFLAG_INVALID,
	/*  72 empty                */	TDS_TYPEFLAG_INVALID,
	/*  73 empty                */	TDS_TYPEFLAG_INVALID,
	/*  74 empty                */	TDS_TYPEFLAG_INVALID,
	/*  75 empty                */	TDS_TYPEFLAG_INVALID,
	/*  76 empty                */	TDS_TYPEFLAG_INVALID,
	/*  77 empty                */	TDS_TYPEFLAG_INVALID,
	/*  78 empty                */	TDS_TYPEFLAG_INVALID,
	/*  79 empty                */	TDS_TYPEFLAG_INVALID,
	/*  80 empty                */	TDS_TYPEFLAG_INVALID,
	/*  81 empty                */	TDS_TYPEFLAG_INVALID,
	/*  82 empty                */	TDS_TYPEFLAG_INVALID,
	/*  83 empty                */	TDS_TYPEFLAG_INVALID,
	/*  84 empty                */	TDS_TYPEFLAG_INVALID,
	/*  85 empty                */	TDS_TYPEFLAG_INVALID,
	/*  86 empty                */	TDS_TYPEFLAG_INVALID,
	/*  87 empty                */	TDS_TYPEFLAG_INVALID,
	/*  88 empty                */	TDS_TYPEFLAG_INVALID,
	/*  89 empty                */	TDS_TYPEFLAG_INVALID,
	/*  90 empty                */	TDS_TYPEFLAG_INVALID,
	/*  91 empty                */	TDS_TYPEFLAG_INVALID,
	/*  92 empty                */	TDS_TYPEFLAG_INVALID,
	/*  93 empty                */	TDS_TYPEFLAG_INVALID,
	/*  94 empty                */	TDS_TYPEFLAG_INVALID,
	/*  95 empty                */	TDS_TYPEFLAG_INVALID,
	/*  96 empty                */	TDS_TYPEFLAG_INVALID,
	/*  97 empty                */	TDS_TYPEFLAG_INVALID,
	/*  98 SYBVARIANT           */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/*  99 SYBNTEXT             */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_COLLATE|TDS_TYPEFLAG_UNICODE,
	/* 100 empty                */	TDS_TYPEFLAG_INVALID,
	/* 101 empty                */	TDS_TYPEFLAG_INVALID,
	/* 102 empty                */	TDS_TYPEFLAG_INVALID,
	/* 103 SYBNVARCHAR          */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_UNICODE,
	/* 104 SYBBITN              */	TDS_TYPEFLAG_NULLABLE,
	/* 105 empty                */	TDS_TYPEFLAG_INVALID,
	/* 106 SYBDECIMAL           */	TDS_TYPEFLAG_NUMERIC,
	/* 107 empty                */	TDS_TYPEFLAG_INVALID,
	/* 108 SYBNUMERIC           */	TDS_TYPEFLAG_NUMERIC,
	/* 109 SYBFLTN              */	TDS_TYPEFLAG_NULLABLE,
	/* 110 SYBMONEYN            */	TDS_TYPEFLAG_NULLABLE,
	/* 111 SYBDATETIMN          */	TDS_TYPEFLAG_NULLABLE,
	/* 112 empty                */	TDS_TYPEFLAG_INVALID,
	/* 113 empty                */	TDS_TYPEFLAG_INVALID,
	/* 114 empty                */	TDS_TYPEFLAG_INVALID,
	/* 115 empty                */	TDS_TYPEFLAG_INVALID,
	/* 116 empty                */	TDS_TYPEFLAG_INVALID,
	/* 117 empty                */	TDS_TYPEFLAG_INVALID,
	/* 118 empty                */	TDS_TYPEFLAG_INVALID,
	/* 119 empty                */	TDS_TYPEFLAG_INVALID,
	/* 120 empty                */	TDS_TYPEFLAG_INVALID,
	/* 121 empty                */	TDS_TYPEFLAG_INVALID,
	/* 122 SYBMONEY4            */	TDS_TYPEFLAG_FIXED,
	/* 123 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE,
	/* 124 empty                */	TDS_TYPEFLAG_INVALID,
	/* 125 empty                */	TDS_TYPEFLAG_INVALID,
	/* 126 empty                */	TDS_TYPEFLAG_INVALID,
	/* 127 SYBINT8              */	TDS_TYPEFLAG_FIXED,
	/* 128 empty                */	TDS_TYPEFLAG_INVALID,
	/* 129 empty                */	TDS_TYPEFLAG_INVALID,
	/* 130 empty                */	TDS_TYPEFLAG_INVALID,
	/* 131 empty                */	TDS_TYPEFLAG_INVALID,
	/* 132 empty                */	TDS_TYPEFLAG_INVALID,
	/* 133 empty                */	TDS_TYPEFLAG_INVALID,
	/* 134 empty                */	TDS_TYPEFLAG_INVALID,
	/* 135 empty                */	TDS_TYPEFLAG_INVALID,
	/* 136 empty                */	TDS_TYPEFLAG_INVALID,
	/* 137 empty                */	TDS_TYPEFLAG_INVALID,
	/* 138 empty                */	TDS_TYPEFLAG_INVALID,
	/* 139 empty                */	TDS_TYPEFLAG_INVALID,
	/* 140 empty                */	TDS_TYPEFLAG_INVALID,
	/* 141 empty                */	TDS_TYPEFLAG_INVALID,
	/* 142 empty                */	TDS_TYPEFLAG_INVALID,
	/* 143 empty                */	TDS_TYPEFLAG_INVALID,
	/* 144 empty                */	TDS_TYPEFLAG_INVALID,
	/* 145 empty                */	TDS_TYPEFLAG_INVALID,
	/* 146 empty                */	TDS_TYPEFLAG_INVALID,
	/* 147 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE,
	/* 148 empty                */	TDS_TYPEFLAG_INVALID,
	/* 149 empty                */	TDS_TYPEFLAG_INVALID,
	/* 150 empty                */	TDS_TYPEFLAG_INVALID,
	/* 151 empty                */	TDS_TYPEFLAG_INVALID,
	/* 152 empty                */	TDS_TYPEFLAG_INVALID,
	/* 153 empty                */	TDS_TYPEFLAG_INVALID,
	/* 154 empty                */	TDS_TYPEFLAG_INVALID,
	/* 155 empty                */	TDS_TYPEFLAG_INVALID,
	/* 156 empty                */	TDS_TYPEFLAG_INVALID,
	/* 157 empty                */	TDS_TYPEFLAG_INVALID,
	/* 158 empty                */	TDS_TYPEFLAG_INVALID,
	/* 159 empty                */	TDS_TYPEFLAG_INVALID,
	/* 160 empty                */	TDS_TYPEFLAG_INVALID,
	/* 161 empty                */	TDS_TYPEFLAG_INVALID,
	/* 162 empty                */	TDS_TYPEFLAG_INVALID,
	/* 163 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 164 empty                */	TDS_TYPEFLAG_INVALID,
	/* 165 XSYBVARBINARY        */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 166 empty                */	TDS_TYPEFLAG_INVALID,
	/* 167 XSYBVARCHAR          */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_COLLATE|TDS_TYPEFLAG_ASCII,
	/* 168 empty                */	TDS_TYPEFLAG_INVALID,
	/* 169 empty                */	TDS_TYPEFLAG_INVALID,
	/* 170 empty                */	TDS_TYPEFLAG_INVALID,
	/* 171 empty                */	TDS_TYPEFLAG_INVALID,
	/* 172 empty                */	TDS_TYPEFLAG_INVALID,
	/* 173 XSYBBINARY           */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 174 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_UNICODE,
	/* 175 XSYBCHAR             */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_COLLATE|TDS_TYPEFLAG_ASCII,
	/* 176 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/* 177 empty                */	TDS_TYPEFLAG_INVALID,
	/* 178 empty                */	TDS_TYPEFLAG_INVALID,
	/* 179 empty                */	TDS_TYPEFLAG_INVALID,
	/* 180 empty                */	TDS_TYPEFLAG_INVALID,
	/* 181 empty                */	TDS_TYPEFLAG_INVALID,
	/* 182 empty                */	TDS_TYPEFLAG_INVALID,
	/* 183 empty                */	TDS_TYPEFLAG_INVALID,
	/* 184 empty                */	TDS_TYPEFLAG_INVALID,
	/* 185 empty                */	TDS_TYPEFLAG_INVALID,
	/* 186 empty                */	TDS_TYPEFLAG_INVALID,
	/* 187 empty                */	TDS_TYPEFLAG_INVALID,
	/* 188 empty                */	TDS_TYPEFLAG_INVALID,
	/* 189 empty                */	TDS_TYPEFLAG_INVALID,
	/* 190 empty                */	TDS_TYPEFLAG_INVALID,
	/* 191 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/* 192 empty                */	TDS_TYPEFLAG_INVALID,
	/* 193 empty                */	TDS_TYPEFLAG_INVALID,
	/* 194 empty                */	TDS_TYPEFLAG_INVALID,
	/* 195 empty                */	TDS_TYPEFLAG_INVALID,
	/* 196 empty                */	TDS_TYPEFLAG_INVALID,
	/* 197 empty                */	TDS_TYPEFLAG_INVALID,
	/* 198 empty                */	TDS_TYPEFLAG_INVALID,
	/* 199 empty                */	TDS_TYPEFLAG_INVALID,
	/* 200 empty                */	TDS_TYPEFLAG_INVALID,
	/* 201 empty                */	TDS_TYPEFLAG_INVALID,
	/* 202 empty                */	TDS_TYPEFLAG_INVALID,
	/* 203 empty                */	TDS_TYPEFLAG_INVALID,
	/* 204 empty                */	TDS_TYPEFLAG_INVALID,
	/* 205 empty                */	TDS_TYPEFLAG_INVALID,
	/* 206 empty                */	TDS_TYPEFLAG_INVALID,
	/* 207 empty                */	TDS_TYPEFLAG_INVALID,
	/* 208 empty                */	TDS_TYPEFLAG_INVALID,
	/* 209 empty                */	TDS_TYPEFLAG_INVALID,
	/* 210 empty                */	TDS_TYPEFLAG_INVALID,
	/* 211 empty                */	TDS_TYPEFLAG_INVALID,
	/* 212 empty                */	TDS_TYPEFLAG_INVALID,
	/* 213 empty                */	TDS_TYPEFLAG_INVALID,
	/* 214 empty                */	TDS_TYPEFLAG_INVALID,
	/* 215 empty                */	TDS_TYPEFLAG_INVALID,
	/* 216 empty                */	TDS_TYPEFLAG_INVALID,
	/* 217 empty                */	TDS_TYPEFLAG_INVALID,
	/* 218 empty                */	TDS_TYPEFLAG_INVALID,
	/* 219 empty                */	TDS_TYPEFLAG_INVALID,
	/* 220 empty                */	TDS_TYPEFLAG_INVALID,
	/* 221 empty                */	TDS_TYPEFLAG_INVALID,
	/* 222 empty                */	TDS_TYPEFLAG_INVALID,
	/* 223 empty                */	TDS_TYPEFLAG_INVALID,
	/* 224 empty                */	TDS_TYPEFLAG_INVALID,
	/* 225 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 226 empty                */	TDS_TYPEFLAG_INVALID,
	/* 227 empty                */	TDS_TYPEFLAG_INVALID,
	/* 228 empty                */	TDS_TYPEFLAG_INVALID,
	/* 229 empty                */	TDS_TYPEFLAG_INVALID,
	/* 230 empty                */	TDS_TYPEFLAG_INVALID,
	/* 231 XSYBNVARCHAR         */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_COLLATE|TDS_TYPEFLAG_UNICODE,
	/* 232 empty                */	TDS_TYPEFLAG_INVALID,
	/* 233 empty                */	TDS_TYPEFLAG_INVALID,
	/* 234 empty                */	TDS_TYPEFLAG_INVALID,
	/* 235 empty                */	TDS_TYPEFLAG_INVALID,
	/* 236 empty                */	TDS_TYPEFLAG_INVALID,
	/* 237 empty                */	TDS_TYPEFLAG_INVALID,
	/* 238 empty                */	TDS_TYPEFLAG_INVALID,
	/* 239 XSYBNCHAR            */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_COLLATE|TDS_TYPEFLAG_UNICODE,
	/* 240 SYBMSUDT             */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 241 SYBMSXML             */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_UNICODE,
	/* 242 empty                */	TDS_TYPEFLAG_INVALID,
	/* 243 empty                */	TDS_TYPEFLAG_INVALID,
	/* 244 empty                */	TDS_TYPEFLAG_INVALID,
	/* 245 empty                */	TDS_TYPEFLAG_INVALID,
	/* 246 empty                */	TDS_TYPEFLAG_INVALID,
	/* 247 empty                */	TDS_TYPEFLAG_INVALID,
	/* 248 empty                */	TDS_TYPEFLAG_INVALID,
	/* 249 empty                */	TDS_TYPEFLAG_INVALID,
	/* 250 empty                */	TDS_TYPEFLAG_INVALID,
	/* 251 empty                */	TDS_TYPEFLAG_INVALID,
	/* 252 empty                */	TDS_TYPEFLAG_INVALID,
	/* 253 empty                */	TDS_TYPEFLAG_INVALID,
	/* 254 empty                */	TDS_TYPEFLAG_INVALID,
	/* 255 empty                */	TDS_TYPEFLAG_INVALID,
};

#if 0
const unsigned char tds_type_flags_syb[256] = {
	/*   0 empty                */	TDS_TYPEFLAG_INVALID,
	/*   1 empty                */	TDS_TYPEFLAG_INVALID,
	/*   2 empty                */	TDS_TYPEFLAG_INVALID,
	/*   3 empty                */	TDS_TYPEFLAG_INVALID,
	/*   4 empty                */	TDS_TYPEFLAG_INVALID,
	/*   5 empty                */	TDS_TYPEFLAG_INVALID,
	/*   6 empty                */	TDS_TYPEFLAG_INVALID,
	/*   7 empty                */	TDS_TYPEFLAG_INVALID,
	/*   8 empty                */	TDS_TYPEFLAG_INVALID,
	/*   9 empty                */	TDS_TYPEFLAG_INVALID,
	/*  10 empty                */	TDS_TYPEFLAG_INVALID,
	/*  11 empty                */	TDS_TYPEFLAG_INVALID,
	/*  12 empty                */	TDS_TYPEFLAG_INVALID,
	/*  13 empty                */	TDS_TYPEFLAG_INVALID,
	/*  14 empty                */	TDS_TYPEFLAG_INVALID,
	/*  15 empty                */	TDS_TYPEFLAG_INVALID,
	/*  16 empty                */	TDS_TYPEFLAG_INVALID,
	/*  17 empty                */	TDS_TYPEFLAG_INVALID,
	/*  18 empty                */	TDS_TYPEFLAG_INVALID,
	/*  19 empty                */	TDS_TYPEFLAG_INVALID,
	/*  20 empty                */	TDS_TYPEFLAG_INVALID,
	/*  21 empty                */	TDS_TYPEFLAG_INVALID,
	/*  22 empty                */	TDS_TYPEFLAG_INVALID,
	/*  23 empty                */	TDS_TYPEFLAG_INVALID,
	/*  24 empty                */	TDS_TYPEFLAG_INVALID,
	/*  25 empty                */	TDS_TYPEFLAG_INVALID,
	/*  26 empty                */	TDS_TYPEFLAG_INVALID,
	/*  27 empty                */	TDS_TYPEFLAG_INVALID,
	/*  28 empty                */	TDS_TYPEFLAG_INVALID,
	/*  29 empty                */	TDS_TYPEFLAG_INVALID,
	/*  30 empty                */	TDS_TYPEFLAG_INVALID,
	/*  31 SYBVOID              */	TDS_TYPEFLAG_FIXED,
	/*  32 empty                */	TDS_TYPEFLAG_INVALID,
	/*  33 empty                */	TDS_TYPEFLAG_INVALID,
	/*  34 SYBIMAGE             */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/*  35 SYBTEXT              */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_COLLATE|TDS_TYPEFLAG_ASCII,
	/*  36 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED|TDS_TYPEFLAG_NULLABLE,
	/*  37 SYBVARBINARY         */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/*  38 SYBINTN              */	TDS_TYPEFLAG_NULLABLE,
	/*  39 SYBVARCHAR           */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_ASCII,
	/*  40 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED|TDS_TYPEFLAG_NULLABLE,
	/*  41 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE,
	/*  42 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE,
	/*  43 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE,
	/*  44 empty                */	TDS_TYPEFLAG_INVALID,
	/*  45 SYBBINARY            */	TDS_TYPEFLAG_VARIABLE,
	/*  46 SYBINTERVAL          */	TDS_TYPEFLAG_FIXED,
	/*  47 SYBCHAR              */	TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_ASCII,
	/*  48 SYBINT1              */	TDS_TYPEFLAG_FIXED,
	/*  49 SYBDATE              */	TDS_TYPEFLAG_FIXED,
	/*  50 SYBBIT               */	TDS_TYPEFLAG_FIXED,
	/*  51 SYBTIME              */	TDS_TYPEFLAG_FIXED,
	/*  52 SYBINT2              */	TDS_TYPEFLAG_FIXED,
	/*  53 empty                */	TDS_TYPEFLAG_INVALID,
	/*  54 empty                */	TDS_TYPEFLAG_INVALID,
	/*  55 empty                */	TDS_TYPEFLAG_INVALID,
	/*  56 SYBINT4              */	TDS_TYPEFLAG_FIXED,
	/*  57 empty                */	TDS_TYPEFLAG_INVALID,
	/*  58 SYBDATETIME4         */	TDS_TYPEFLAG_FIXED,
	/*  59 SYBREAL              */	TDS_TYPEFLAG_FIXED,
	/*  60 SYBMONEY             */	TDS_TYPEFLAG_FIXED,
	/*  61 SYBDATETIME          */	TDS_TYPEFLAG_FIXED,
	/*  62 SYBFLT8              */	TDS_TYPEFLAG_FIXED,
	/*  63 empty                */	TDS_TYPEFLAG_INVALID,
	/*  64 SYBUINT1             */	TDS_TYPEFLAG_FIXED,
	/*  65 SYBUINT2             */	TDS_TYPEFLAG_FIXED,
	/*  66 SYBUINT4             */	TDS_TYPEFLAG_FIXED,
	/*  67 SYBUINT8             */	TDS_TYPEFLAG_FIXED,
	/*  68 SYBUINTN             */	TDS_TYPEFLAG_NULLABLE,
	/*  69 empty                */	TDS_TYPEFLAG_INVALID,
	/*  70 empty                */	TDS_TYPEFLAG_INVALID,
	/*  71 empty                */	TDS_TYPEFLAG_INVALID,
	/*  72 empty                */	TDS_TYPEFLAG_INVALID,
	/*  73 empty                */	TDS_TYPEFLAG_INVALID,
	/*  74 empty                */	TDS_TYPEFLAG_INVALID,
	/*  75 empty                */	TDS_TYPEFLAG_INVALID,
	/*  76 empty                */	TDS_TYPEFLAG_INVALID,
	/*  77 empty                */	TDS_TYPEFLAG_INVALID,
	/*  78 empty                */	TDS_TYPEFLAG_INVALID,
	/*  79 empty                */	TDS_TYPEFLAG_INVALID,
	/*  80 empty                */	TDS_TYPEFLAG_INVALID,
	/*  81 empty                */	TDS_TYPEFLAG_INVALID,
	/*  82 empty                */	TDS_TYPEFLAG_INVALID,
	/*  83 empty                */	TDS_TYPEFLAG_INVALID,
	/*  84 empty                */	TDS_TYPEFLAG_INVALID,
	/*  85 empty                */	TDS_TYPEFLAG_INVALID,
	/*  86 empty                */	TDS_TYPEFLAG_INVALID,
	/*  87 empty                */	TDS_TYPEFLAG_INVALID,
	/*  88 empty                */	TDS_TYPEFLAG_INVALID,
	/*  89 empty                */	TDS_TYPEFLAG_INVALID,
	/*  90 empty                */	TDS_TYPEFLAG_INVALID,
	/*  91 empty                */	TDS_TYPEFLAG_INVALID,
	/*  92 empty                */	TDS_TYPEFLAG_INVALID,
	/*  93 empty                */	TDS_TYPEFLAG_INVALID,
	/*  94 empty                */	TDS_TYPEFLAG_INVALID,
	/*  95 empty                */	TDS_TYPEFLAG_INVALID,
	/*  96 empty                */	TDS_TYPEFLAG_INVALID,
	/*  97 empty                */	TDS_TYPEFLAG_INVALID,
	/*  98 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/*  99 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_UNICODE,
	/* 100 empty                */	TDS_TYPEFLAG_INVALID,
	/* 101 empty                */	TDS_TYPEFLAG_INVALID,
	/* 102 empty                */	TDS_TYPEFLAG_INVALID,
	/* 103 SYBSENSITIVITY       */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_ASCII,
	/* 104 SYBBOUNDARY          */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_ASCII,
	/* 105 empty                */	TDS_TYPEFLAG_INVALID,
	/* 106 SYBDECIMAL           */	TDS_TYPEFLAG_NUMERIC,
	/* 107 empty                */	TDS_TYPEFLAG_INVALID,
	/* 108 SYBNUMERIC           */	TDS_TYPEFLAG_NUMERIC,
	/* 109 SYBFLTN              */	TDS_TYPEFLAG_NULLABLE,
	/* 110 SYBMONEYN            */	TDS_TYPEFLAG_NULLABLE,
	/* 111 SYBDATETIMN          */	TDS_TYPEFLAG_NULLABLE,
	/* 112 empty                */	TDS_TYPEFLAG_INVALID,
	/* 113 empty                */	TDS_TYPEFLAG_INVALID,
	/* 114 empty                */	TDS_TYPEFLAG_INVALID,
	/* 115 empty                */	TDS_TYPEFLAG_INVALID,
	/* 116 empty                */	TDS_TYPEFLAG_INVALID,
	/* 117 empty                */	TDS_TYPEFLAG_INVALID,
	/* 118 empty                */	TDS_TYPEFLAG_INVALID,
	/* 119 empty                */	TDS_TYPEFLAG_INVALID,
	/* 120 empty                */	TDS_TYPEFLAG_INVALID,
	/* 121 empty                */	TDS_TYPEFLAG_INVALID,
	/* 122 SYBMONEY4            */	TDS_TYPEFLAG_FIXED,
	/* 123 SYBDATEN             */	TDS_TYPEFLAG_NULLABLE,
	/* 124 empty                */	TDS_TYPEFLAG_INVALID,
	/* 125 empty                */	TDS_TYPEFLAG_INVALID,
	/* 126 empty                */	TDS_TYPEFLAG_INVALID,
	/* 127 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_FIXED,
	/* 128 empty                */	TDS_TYPEFLAG_INVALID,
	/* 129 empty                */	TDS_TYPEFLAG_INVALID,
	/* 130 empty                */	TDS_TYPEFLAG_INVALID,
	/* 131 empty                */	TDS_TYPEFLAG_INVALID,
	/* 132 empty                */	TDS_TYPEFLAG_INVALID,
	/* 133 empty                */	TDS_TYPEFLAG_INVALID,
	/* 134 empty                */	TDS_TYPEFLAG_INVALID,
	/* 135 empty                */	TDS_TYPEFLAG_INVALID,
	/* 136 empty                */	TDS_TYPEFLAG_INVALID,
	/* 137 empty                */	TDS_TYPEFLAG_INVALID,
	/* 138 empty                */	TDS_TYPEFLAG_INVALID,
	/* 139 empty                */	TDS_TYPEFLAG_INVALID,
	/* 140 empty                */	TDS_TYPEFLAG_INVALID,
	/* 141 empty                */	TDS_TYPEFLAG_INVALID,
	/* 142 empty                */	TDS_TYPEFLAG_INVALID,
	/* 143 empty                */	TDS_TYPEFLAG_INVALID,
	/* 144 empty                */	TDS_TYPEFLAG_INVALID,
	/* 145 empty                */	TDS_TYPEFLAG_INVALID,
	/* 146 empty                */	TDS_TYPEFLAG_INVALID,
	/* 147 SYBTIMEN             */	TDS_TYPEFLAG_NULLABLE,
	/* 148 empty                */	TDS_TYPEFLAG_INVALID,
	/* 149 empty                */	TDS_TYPEFLAG_INVALID,
	/* 150 empty                */	TDS_TYPEFLAG_INVALID,
	/* 151 empty                */	TDS_TYPEFLAG_INVALID,
	/* 152 empty                */	TDS_TYPEFLAG_INVALID,
	/* 153 empty                */	TDS_TYPEFLAG_INVALID,
	/* 154 empty                */	TDS_TYPEFLAG_INVALID,
	/* 155 empty                */	TDS_TYPEFLAG_INVALID,
	/* 156 empty                */	TDS_TYPEFLAG_INVALID,
	/* 157 empty                */	TDS_TYPEFLAG_INVALID,
	/* 158 empty                */	TDS_TYPEFLAG_INVALID,
	/* 159 empty                */	TDS_TYPEFLAG_INVALID,
	/* 160 empty                */	TDS_TYPEFLAG_INVALID,
	/* 161 empty                */	TDS_TYPEFLAG_INVALID,
	/* 162 empty                */	TDS_TYPEFLAG_INVALID,
	/* 163 SYBXML               */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 164 empty                */	TDS_TYPEFLAG_INVALID,
	/* 165 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 166 empty                */	TDS_TYPEFLAG_INVALID,
	/* 167 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_ASCII,
	/* 168 empty                */	TDS_TYPEFLAG_INVALID,
	/* 169 empty                */	TDS_TYPEFLAG_INVALID,
	/* 170 empty                */	TDS_TYPEFLAG_INVALID,
	/* 171 empty                */	TDS_TYPEFLAG_INVALID,
	/* 172 empty                */	TDS_TYPEFLAG_INVALID,
	/* 173 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 174 SYBUNITEXT           */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_UNICODE,
	/* 175 SYBLONGCHAR          */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_ASCII,
	/* 176 SYBSINT1             */	TDS_TYPEFLAG_FIXED,
	/* 177 empty                */	TDS_TYPEFLAG_INVALID,
	/* 178 empty                */	TDS_TYPEFLAG_INVALID,
	/* 179 empty                */	TDS_TYPEFLAG_INVALID,
	/* 180 empty                */	TDS_TYPEFLAG_INVALID,
	/* 181 empty                */	TDS_TYPEFLAG_INVALID,
	/* 182 empty                */	TDS_TYPEFLAG_INVALID,
	/* 183 empty                */	TDS_TYPEFLAG_INVALID,
	/* 184 empty                */	TDS_TYPEFLAG_INVALID,
	/* 185 empty                */	TDS_TYPEFLAG_INVALID,
	/* 186 empty                */	TDS_TYPEFLAG_INVALID,
	/* 187 empty                */	TDS_TYPEFLAG_INVALID,
	/* 188 empty                */	TDS_TYPEFLAG_INVALID,
	/* 189 empty                */	TDS_TYPEFLAG_INVALID,
	/* 190 empty                */	TDS_TYPEFLAG_INVALID,
	/* 191 SYB5INT8             */	TDS_TYPEFLAG_FIXED,
	/* 192 empty                */	TDS_TYPEFLAG_INVALID,
	/* 193 empty                */	TDS_TYPEFLAG_INVALID,
	/* 194 empty                */	TDS_TYPEFLAG_INVALID,
	/* 195 empty                */	TDS_TYPEFLAG_INVALID,
	/* 196 empty                */	TDS_TYPEFLAG_INVALID,
	/* 197 empty                */	TDS_TYPEFLAG_INVALID,
	/* 198 empty                */	TDS_TYPEFLAG_INVALID,
	/* 199 empty                */	TDS_TYPEFLAG_INVALID,
	/* 200 empty                */	TDS_TYPEFLAG_INVALID,
	/* 201 empty                */	TDS_TYPEFLAG_INVALID,
	/* 202 empty                */	TDS_TYPEFLAG_INVALID,
	/* 203 empty                */	TDS_TYPEFLAG_INVALID,
	/* 204 empty                */	TDS_TYPEFLAG_INVALID,
	/* 205 empty                */	TDS_TYPEFLAG_INVALID,
	/* 206 empty                */	TDS_TYPEFLAG_INVALID,
	/* 207 empty                */	TDS_TYPEFLAG_INVALID,
	/* 208 empty                */	TDS_TYPEFLAG_INVALID,
	/* 209 empty                */	TDS_TYPEFLAG_INVALID,
	/* 210 empty                */	TDS_TYPEFLAG_INVALID,
	/* 211 empty                */	TDS_TYPEFLAG_INVALID,
	/* 212 empty                */	TDS_TYPEFLAG_INVALID,
	/* 213 empty                */	TDS_TYPEFLAG_INVALID,
	/* 214 empty                */	TDS_TYPEFLAG_INVALID,
	/* 215 empty                */	TDS_TYPEFLAG_INVALID,
	/* 216 empty                */	TDS_TYPEFLAG_INVALID,
	/* 217 empty                */	TDS_TYPEFLAG_INVALID,
	/* 218 empty                */	TDS_TYPEFLAG_INVALID,
	/* 219 empty                */	TDS_TYPEFLAG_INVALID,
	/* 220 empty                */	TDS_TYPEFLAG_INVALID,
	/* 221 empty                */	TDS_TYPEFLAG_INVALID,
	/* 222 empty                */	TDS_TYPEFLAG_INVALID,
	/* 223 empty                */	TDS_TYPEFLAG_INVALID,
	/* 224 empty                */	TDS_TYPEFLAG_INVALID,
	/* 225 SYBLONGBINARY        */	TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 226 empty                */	TDS_TYPEFLAG_INVALID,
	/* 227 empty                */	TDS_TYPEFLAG_INVALID,
	/* 228 empty                */	TDS_TYPEFLAG_INVALID,
	/* 229 empty                */	TDS_TYPEFLAG_INVALID,
	/* 230 empty                */	TDS_TYPEFLAG_INVALID,
	/* 231 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_UNICODE,
	/* 232 empty                */	TDS_TYPEFLAG_INVALID,
	/* 233 empty                */	TDS_TYPEFLAG_INVALID,
	/* 234 empty                */	TDS_TYPEFLAG_INVALID,
	/* 235 empty                */	TDS_TYPEFLAG_INVALID,
	/* 236 empty                */	TDS_TYPEFLAG_INVALID,
	/* 237 empty                */	TDS_TYPEFLAG_INVALID,
	/* 238 empty                */	TDS_TYPEFLAG_INVALID,
	/* 239 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_UNICODE,
	/* 240 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE,
	/* 241 empty                */	TDS_TYPEFLAG_INVALID|TDS_TYPEFLAG_NULLABLE|TDS_TYPEFLAG_VARIABLE|TDS_TYPEFLAG_UNICODE,
	/* 242 empty                */	TDS_TYPEFLAG_INVALID,
	/* 243 empty                */	TDS_TYPEFLAG_INVALID,
	/* 244 empty                */	TDS_TYPEFLAG_INVALID,
	/* 245 empty                */	TDS_TYPEFLAG_INVALID,
	/* 246 empty                */	TDS_TYPEFLAG_INVALID,
	/* 247 empty                */	TDS_TYPEFLAG_INVALID,
	/* 248 empty                */	TDS_TYPEFLAG_INVALID,
	/* 249 empty                */	TDS_TYPEFLAG_INVALID,
	/* 250 empty                */	TDS_TYPEFLAG_INVALID,
	/* 251 empty                */	TDS_TYPEFLAG_INVALID,
	/* 252 empty                */	TDS_TYPEFLAG_INVALID,
	/* 253 empty                */	TDS_TYPEFLAG_INVALID,
	/* 254 empty                */	TDS_TYPEFLAG_INVALID,
	/* 255 empty                */	TDS_TYPEFLAG_INVALID,
};

const char *const tds_type_names[256] = {
	/*   0 */	"",
	/*   1 */	"",
	/*   2 */	"",
	/*   3 */	"",
	/*   4 */	"",
	/*   5 */	"",
	/*   6 */	"",
	/*   7 */	"",
	/*   8 */	"",
	/*   9 */	"",
	/*  10 */	"",
	/*  11 */	"",
	/*  12 */	"",
	/*  13 */	"",
	/*  14 */	"",
	/*  15 */	"",
	/*  16 */	"",
	/*  17 */	"",
	/*  18 */	"",
	/*  19 */	"",
	/*  20 */	"",
	/*  21 */	"",
	/*  22 */	"",
	/*  23 */	"",
	/*  24 */	"",
	/*  25 */	"",
	/*  26 */	"",
	/*  27 */	"",
	/*  28 */	"",
	/*  29 */	"",
	/*  30 */	"",
	/*  31 */	"SYBVOID",
	/*  32 */	"",
	/*  33 */	"",
	/*  34 */	"SYBIMAGE",
	/*  35 */	"SYBTEXT",
	/*  36 */	"SYBUNIQUE",
	/*  37 */	"SYBVARBINARY",
	/*  38 */	"SYBINTN",
	/*  39 */	"SYBVARCHAR",
	/*  40 */	"SYBMSDATE",
	/*  41 */	"SYBMSTIME",
	/*  42 */	"SYBMSDATETIME2",
	/*  43 */	"SYBMSDATETIMEOFFSET",
	/*  44 */	"",
	/*  45 */	"SYBBINARY",
	/*  46 */	"SYBINTERVAL",
	/*  47 */	"SYBCHAR",
	/*  48 */	"SYBINT1",
	/*  49 */	"SYBDATE",
	/*  50 */	"SYBBIT",
	/*  51 */	"SYBTIME",
	/*  52 */	"SYBINT2",
	/*  53 */	"",
	/*  54 */	"",
	/*  55 */	"",
	/*  56 */	"SYBINT4",
	/*  57 */	"",
	/*  58 */	"SYBDATETIME4",
	/*  59 */	"SYBREAL",
	/*  60 */	"SYBMONEY",
	/*  61 */	"SYBDATETIME",
	/*  62 */	"SYBFLT8",
	/*  63 */	"",
	/*  64 */	"SYBUINT1",
	/*  65 */	"SYBUINT2",
	/*  66 */	"SYBUINT4",
	/*  67 */	"SYBUINT8",
	/*  68 */	"SYBUINTN",
	/*  69 */	"",
	/*  70 */	"",
	/*  71 */	"",
	/*  72 */	"",
	/*  73 */	"",
	/*  74 */	"",
	/*  75 */	"",
	/*  76 */	"",
	/*  77 */	"",
	/*  78 */	"",
	/*  79 */	"",
	/*  80 */	"",
	/*  81 */	"",
	/*  82 */	"",
	/*  83 */	"",
	/*  84 */	"",
	/*  85 */	"",
	/*  86 */	"",
	/*  87 */	"",
	/*  88 */	"",
	/*  89 */	"",
	/*  90 */	"",
	/*  91 */	"",
	/*  92 */	"",
	/*  93 */	"",
	/*  94 */	"",
	/*  95 */	"",
	/*  96 */	"",
	/*  97 */	"",
	/*  98 */	"SYBVARIANT",
	/*  99 */	"SYBNTEXT",
	/* 100 */	"",
	/* 101 */	"",
	/* 102 */	"",
	/* 103 */	"SYBNVARCHAR or SYBSENSITIVITY",
	/* 104 */	"SYBBITN or SYBBOUNDARY",
	/* 105 */	"",
	/* 106 */	"SYBDECIMAL",
	/* 107 */	"",
	/* 108 */	"SYBNUMERIC",
	/* 109 */	"SYBFLTN",
	/* 110 */	"SYBMONEYN",
	/* 111 */	"SYBDATETIMN",
	/* 112 */	"",
	/* 113 */	"",
	/* 114 */	"",
	/* 115 */	"",
	/* 116 */	"",
	/* 117 */	"",
	/* 118 */	"",
	/* 119 */	"",
	/* 120 */	"",
	/* 121 */	"",
	/* 122 */	"SYBMONEY4",
	/* 123 */	"SYBDATEN",
	/* 124 */	"",
	/* 125 */	"",
	/* 126 */	"",
	/* 127 */	"SYBINT8",
	/* 128 */	"",
	/* 129 */	"",
	/* 130 */	"",
	/* 131 */	"",
	/* 132 */	"",
	/* 133 */	"",
	/* 134 */	"",
	/* 135 */	"",
	/* 136 */	"",
	/* 137 */	"",
	/* 138 */	"",
	/* 139 */	"",
	/* 140 */	"",
	/* 141 */	"",
	/* 142 */	"",
	/* 143 */	"",
	/* 144 */	"",
	/* 145 */	"",
	/* 146 */	"",
	/* 147 */	"SYBTIMEN",
	/* 148 */	"",
	/* 149 */	"",
	/* 150 */	"",
	/* 151 */	"",
	/* 152 */	"",
	/* 153 */	"",
	/* 154 */	"",
	/* 155 */	"",
	/* 156 */	"",
	/* 157 */	"",
	/* 158 */	"",
	/* 159 */	"",
	/* 160 */	"",
	/* 161 */	"",
	/* 162 */	"",
	/* 163 */	"SYBXML",
	/* 164 */	"",
	/* 165 */	"XSYBVARBINARY",
	/* 166 */	"",
	/* 167 */	"XSYBVARCHAR",
	/* 168 */	"",
	/* 169 */	"",
	/* 170 */	"",
	/* 171 */	"",
	/* 172 */	"",
	/* 173 */	"XSYBBINARY",
	/* 174 */	"SYBUNITEXT",
	/* 175 */	"XSYBCHAR or SYBLONGCHAR",
	/* 176 */	"SYBSINT1",
	/* 177 */	"",
	/* 178 */	"",
	/* 179 */	"",
	/* 180 */	"",
	/* 181 */	"",
	/* 182 */	"",
	/* 183 */	"",
	/* 184 */	"",
	/* 185 */	"",
	/* 186 */	"",
	/* 187 */	"",
	/* 188 */	"",
	/* 189 */	"",
	/* 190 */	"",
	/* 191 */	"SYB5INT8",
	/* 192 */	"",
	/* 193 */	"",
	/* 194 */	"",
	/* 195 */	"",
	/* 196 */	"",
	/* 197 */	"",
	/* 198 */	"",
	/* 199 */	"",
	/* 200 */	"",
	/* 201 */	"",
	/* 202 */	"",
	/* 203 */	"",
	/* 204 */	"",
	/* 205 */	"",
	/* 206 */	"",
	/* 207 */	"",
	/* 208 */	"",
	/* 209 */	"",
	/* 210 */	"",
	/* 211 */	"",
	/* 212 */	"",
	/* 213 */	"",
	/* 214 */	"",
	/* 215 */	"",
	/* 216 */	"",
	/* 217 */	"",
	/* 218 */	"",
	/* 219 */	"",
	/* 220 */	"",
	/* 221 */	"",
	/* 222 */	"",
	/* 223 */	"",
	/* 224 */	"",
	/* 225 */	"SYBLONGBINARY",
	/* 226 */	"",
	/* 227 */	"",
	/* 228 */	"",
	/* 229 */	"",
	/* 230 */	"",
	/* 231 */	"XSYBNVARCHAR",
	/* 232 */	"",
	/* 233 */	"",
	/* 234 */	"",
	/* 235 */	"",
	/* 236 */	"",
	/* 237 */	"",
	/* 238 */	"",
	/* 239 */	"XSYBNCHAR",
	/* 240 */	"SYBMSUDT",
	/* 241 */	"SYBMSXML",
	/* 242 */	"",
	/* 243 */	"",
	/* 244 */	"",
	/* 245 */	"",
	/* 246 */	"",
	/* 247 */	"",
	/* 248 */	"",
	/* 249 */	"",
	/* 250 */	"",
	/* 251 */	"",
	/* 252 */	"",
	/* 253 */	"",
	/* 254 */	"",
	/* 255 */	"",
};
#endif
