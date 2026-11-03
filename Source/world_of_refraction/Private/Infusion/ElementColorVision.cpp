// ElementColorVision.cpp
// B-294: see ElementColorVision.h.

#include "Infusion/ElementColorVision.h"
#include "Infusion/ElementColors.h"

namespace
{
	using FCvdMatrix = float[3][3];

	// Machado 2009, severity 1.0 (the matrices colorspacious and DaltonLens use).
	constexpr FCvdMatrix MACHADO_PROTAN = {{0.152286f, 1.052583f, -0.204868f}, {0.114503f, 0.786281f, 0.099216f}, {-0.003882f, -0.048116f, 1.051998f}};
	constexpr FCvdMatrix MACHADO_DEUTAN = {{0.367322f, 0.860646f, -0.227968f}, {0.280085f, 0.672501f, 0.047413f}, {-0.011820f, 0.042940f, 0.968881f}};
	constexpr FCvdMatrix MACHADO_TRITAN = {{1.255528f, -0.076749f, -0.178779f}, {-0.078411f, 0.930809f, 0.147602f}, {0.004733f, 0.691367f, 0.303900f}};

	// D65 white; the sRGB -> XYZ matrix is in CvdToLab.
	constexpr double D65_X = 0.95047;
	constexpr double D65_Y = 1.0;
	constexpr double D65_Z = 1.08883;
	constexpr double LAB_EPSILON = 216.0 / 24389.0;
	constexpr double LAB_KAPPA = 24389.0 / 27.0;
	constexpr double CIEDE_25_POW_7 = 6103515625.0; // 25^7
	constexpr int32 PALETTE_PAIRS_SHOWN = 6;

	struct FCvdLab
	{
		double L, A, B;
	};

	FCvdLab CvdToLab(const FLinearColor &C)
	{
		const double X = 0.4124564 * C.R + 0.3575761 * C.G + 0.1804375 * C.B;
		const double Y = 0.2126729 * C.R + 0.7151522 * C.G + 0.0721750 * C.B;
		const double Z = 0.0193339 * C.R + 0.1191920 * C.G + 0.9503041 * C.B;
		auto F = [](double T) { return T > LAB_EPSILON ? FMath::Pow(T, 1.0 / 3.0) : (LAB_KAPPA * T + 16.0) / 116.0; };
		const double FX = F(X / D65_X), FY = F(Y / D65_Y), FZ = F(Z / D65_Z);
		return {116.0 * FY - 16.0, 500.0 * (FX - FY), 200.0 * (FY - FZ)};
	}

	double CvdDeg(double Radians) { return FMath::RadiansToDegrees(Radians); }
	double CvdRad(double Degrees) { return FMath::DegreesToRadians(Degrees); }

	const TCHAR *CvdVisionName(ElementColorVision::EColorVision Vision)
	{
		static const TCHAR *Names[] = {TEXT("normal"), TEXT("protan"), TEXT("deutan"), TEXT("tritan")};
		return Names[static_cast<int32>(Vision)];
	}

	FString CvdHex(const FLinearColor &Linear)
	{
		return FString::Printf(TEXT("#%s"), *Linear.ToFColor(/*bSRGB=*/true).ToHex().Left(6));
	}
}

const TArray<ESpellElement> &ElementColorVision::Elements()
{
	static const TArray<ESpellElement> Nine = {ESpellElement::Fire, ESpellElement::Lightning, ESpellElement::Earth,
											   ESpellElement::Wind, ESpellElement::Water, ESpellElement::Reality,
											   ESpellElement::Void, ESpellElement::Light, ESpellElement::Darkness};
	return Nine;
}

FLinearColor ElementColorVision::Simulate(const FLinearColor &Linear, EColorVision Vision)
{
	const FCvdMatrix *M = Vision == EColorVision::Protan   ? &MACHADO_PROTAN
						: Vision == EColorVision::Deutan ? &MACHADO_DEUTAN
						: Vision == EColorVision::Tritan ? &MACHADO_TRITAN
														 : nullptr;
	if (!M)
	{
		return Linear;
	}
	const float In[3] = {Linear.R, Linear.G, Linear.B};
	float Out[3];
	for (int32 Row = 0; Row < 3; ++Row)
	{
		Out[Row] = FMath::Clamp((*M)[Row][0] * In[0] + (*M)[Row][1] * In[1] + (*M)[Row][2] * In[2], 0.0f, 1.0f);
	}
	return FLinearColor(Out[0], Out[1], Out[2], Linear.A);
}

float ElementColorVision::DeltaE2000(const FLinearColor &ColourA, const FLinearColor &ColourB)
{
	const FCvdLab P = CvdToLab(ColourA), Q = CvdToLab(ColourB);
	return DeltaE2000Lab(FVector(P.L, P.A, P.B), FVector(Q.L, Q.A, Q.B));
}

float ElementColorVision::DeltaE2000Lab(const FVector &LabA, const FVector &LabB)
{
	const FCvdLab P = {LabA.X, LabA.Y, LabA.Z}, Q = {LabB.X, LabB.Y, LabB.Z};
	const double C1 = FMath::Sqrt(P.A * P.A + P.B * P.B), C2 = FMath::Sqrt(Q.A * Q.A + Q.B * Q.B);
	const double CBar7 = FMath::Pow((C1 + C2) / 2.0, 7.0);
	const double G = 0.5 * (1.0 - FMath::Sqrt(CBar7 / (CBar7 + CIEDE_25_POW_7)));
	const double A1 = (1.0 + G) * P.A, A2 = (1.0 + G) * Q.A;
	const double C1P = FMath::Sqrt(A1 * A1 + P.B * P.B), C2P = FMath::Sqrt(A2 * A2 + Q.B * Q.B);
	auto Hue = [](double B, double A) { const double H = CvdDeg(FMath::Atan2(B, A)); return H < 0.0 ? H + 360.0 : H; };
	const double H1 = Hue(P.B, A1), H2 = Hue(Q.B, A2);
	const bool bAchromatic = C1P * C2P == 0.0;

	double DHue = bAchromatic ? 0.0 : H2 - H1;
	DHue = DHue > 180.0 ? DHue - 360.0 : (DHue < -180.0 ? DHue + 360.0 : DHue);
	const double DL = Q.L - P.L, DC = C2P - C1P;
	const double DH = 2.0 * FMath::Sqrt(C1P * C2P) * FMath::Sin(CvdRad(DHue / 2.0));

	const double LBar = (P.L + Q.L) / 2.0, CBarP = (C1P + C2P) / 2.0;
	double HBar = H1 + H2;
	if (!bAchromatic)
	{
		HBar = (H1 + H2) / 2.0;
		if (FMath::Abs(H1 - H2) > 180.0)
		{
			HBar += HBar < 180.0 ? 180.0 : -180.0;
		}
	}
	const double T = 1.0 - 0.17 * FMath::Cos(CvdRad(HBar - 30.0)) + 0.24 * FMath::Cos(CvdRad(2.0 * HBar)) +
					 0.32 * FMath::Cos(CvdRad(3.0 * HBar + 6.0)) - 0.20 * FMath::Cos(CvdRad(4.0 * HBar - 63.0));
	const double DTheta = 30.0 * FMath::Exp(-FMath::Square((HBar - 275.0) / 25.0));
	const double CBarP7 = FMath::Pow(CBarP, 7.0);
	const double RC = 2.0 * FMath::Sqrt(CBarP7 / (CBarP7 + CIEDE_25_POW_7));
	const double SL = 1.0 + 0.015 * FMath::Square(LBar - 50.0) / FMath::Sqrt(20.0 + FMath::Square(LBar - 50.0));
	const double SC = 1.0 + 0.045 * CBarP;
	const double SH = 1.0 + 0.015 * CBarP * T;
	const double RT = -FMath::Sin(CvdRad(2.0 * DTheta)) * RC;
	return static_cast<float>(FMath::Sqrt(FMath::Square(DL / SL) + FMath::Square(DC / SC) + FMath::Square(DH / SH) +
										  RT * (DC / SC) * (DH / SH)));
}

TArray<ElementColorVision::FPairDistance> ElementColorVision::ElementPairsClosestFirst()
{
	static const EColorVision Visions[] = {EColorVision::Normal, EColorVision::Protan, EColorVision::Deutan, EColorVision::Tritan};
	const TArray<ESpellElement> &Nine = Elements();
	TArray<FPairDistance> Pairs;
	for (int32 I = 0; I < Nine.Num(); ++I)
	{
		for (int32 J = I + 1; J < Nine.Num(); ++J)
		{
			FPairDistance Pair;
			Pair.A = Nine[I];
			Pair.B = Nine[J];
			Pair.DeltaE = TNumericLimits<float>::Max();
			for (const EColorVision Vision : Visions)
			{
				const float DeltaE = DeltaE2000(Simulate(ElementColors::GetColorForElement(Nine[I]), Vision),
												Simulate(ElementColors::GetColorForElement(Nine[J]), Vision));
				if (DeltaE < Pair.DeltaE)
				{
					Pair.DeltaE = DeltaE;
					Pair.Where = Vision;
				}
			}
			Pairs.Add(Pair);
		}
	}
	Pairs.Sort([](const FPairDistance &L, const FPairDistance &R) { return L.DeltaE < R.DeltaE; });
	return Pairs;
}

FString ElementColorVision::GetElementPaletteString()
{
	FString Out = TEXT("Element palette (normal / protan / deutan / tritan):\n");
	for (const ESpellElement Element : Elements())
	{
		const FLinearColor Colour = ElementColors::GetColorForElement(Element);
		Out += FString::Printf(TEXT("  %-10s %s / %s / %s / %s%s\n"), *UEnum::GetDisplayValueAsText(Element).ToString(), *CvdHex(Colour),
							   *CvdHex(Simulate(Colour, EColorVision::Protan)), *CvdHex(Simulate(Colour, EColorVision::Deutan)),
							   *CvdHex(Simulate(Colour, EColorVision::Tritan)),
							   ElementColors::NeedsTextOutline(Element) ? TEXT("  (text outlined)") : TEXT(""));
	}
	Out += FString::Printf(TEXT("  Ice %s, Physical %s\nClosest pairs (readable at %.0f and up):\n"), *CvdHex(ElementColors::Ice),
						   *CvdHex(ElementColors::Physical), READABLE_DELTA_E);
	const TArray<FPairDistance> Pairs = ElementPairsClosestFirst();
	for (int32 Index = 0; Index < FMath::Min(PALETTE_PAIRS_SHOWN, Pairs.Num()); ++Index)
	{
		const FPairDistance &Pair = Pairs[Index];
		Out += FString::Printf(TEXT("  %s / %s: %.1f (%s)\n"), *UEnum::GetDisplayValueAsText(Pair.A).ToString(),
							   *UEnum::GetDisplayValueAsText(Pair.B).ToString(), Pair.DeltaE, CvdVisionName(Pair.Where));
	}
	return Out;
}
