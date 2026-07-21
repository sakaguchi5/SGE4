#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"

#include <bit>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

namespace h=sge4_5::spiral2::hierarchy;
namespace k=sge4_5::spiral2::contracts;
namespace c=sge4_5::spiral2::corpus;
namespace b=sge4_5::base;

int main(int argc,char** argv)
{
    auto topologies=c::BuildTopologyCorpusV1(); if(!topologies) return 1;
    std::size_t combinations=0;
    for(const auto& topology:topologies.Value())
    {
        auto frames=c::BuildFrameCorpusV1(topology.hierarchy); if(!frames||frames.Value().size()!=12) return 2;
        const auto schema=k::BuildDynamicPaletteInvocationSchemaV1(topology.hierarchy);
        if(schema.requiredBytes!=topology.hierarchy.boneCount*32u||schema.alignment!=16u) return 3;
        const auto frozenBytes=k::SerializeDynamicPaletteInvocationSchemaV1(schema);
        for(const auto& frame:frames.Value())
        {
            auto bytes=k::SerializeDynamicLocalMotorPaletteV1(frame.palette);
            if(bytes.size()!=schema.requiredBytes) return 4;
            auto decoded=k::DeserializeDynamicLocalMotorPaletteV1(schema.boneCount,bytes);
            if(!decoded||k::SerializeDynamicLocalMotorPaletteV1(decoded.Value())!=bytes) return 5;
            auto reference=c::BuildIndependentCpuReferenceV1(topology.hierarchy,frame.palette);
            if(!reference||reference.Value().size()!=static_cast<std::size_t>(schema.boneCount)*5u) return 6;
            for(const auto& point:reference.Value()) if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)||point.w!=1.0f) return 7;
            if(k::SerializeDynamicPaletteInvocationSchemaV1(k::BuildDynamicPaletteInvocationSchemaV1(topology.hierarchy))!=frozenBytes) return 8;
            ++combinations;
        }
        auto first=k::SerializeDynamicLocalMotorPaletteV1(frames.Value()[0].palette);
        auto second=k::SerializeDynamicLocalMotorPaletteV1(frames.Value()[1].palette);
        if(first==second&&topology.hierarchy.boneCount>0) return 9;
        if(k::DeserializeDynamicLocalMotorPaletteV1(schema.boneCount,std::span<const std::byte>(first).first(first.size()-1))) return 10;
        if(k::DeserializeDynamicLocalMotorPaletteV1(schema.boneCount+1,first)) return 11;
    }
    std::array<k::RotationTranslationV1,1> invalid{};
    invalid[0].qw=0.0;
    if(k::BuildDynamicLocalMotorPaletteV1(1,invalid)) return 12;
    invalid[0].qw=std::numeric_limits<double>::quiet_NaN();
    if(k::BuildDynamicLocalMotorPaletteV1(1,invalid)) return 13;
    invalid[0]={-1.0,0,0,0,1,2,3};
    auto canonical=k::BuildDynamicLocalMotorPaletteV1(1,invalid); if(!canonical||canonical.Value().motors[0].qr[0]!=1.0f) return 14;
    invalid[0]={2.0,0,0,0,0,0,0};
    auto normalized=k::BuildDynamicLocalMotorPaletteV1(1,invalid);
    if(!normalized||normalized.Value().motors[0].qr[0]!=1.0f||
       !k::ValidateDynamicLocalMotorPaletteV1(normalized.Value(),1)) return 15;
    auto nonUnit=normalized.Value();
    nonUnit.motors[0].qr={2.0f,0.0f,0.0f,0.0f};
    if(k::ValidateDynamicLocalMotorPaletteV1(nonUnit,1)) return 16;
    invalid[0]={-1.0e-13,1.0,0,0,0,0,0};
    auto signBoundary=k::BuildDynamicLocalMotorPaletteV1(1,invalid);
    if(!signBoundary||!k::ValidateDynamicLocalMotorPaletteV1(signBoundary.Value(),1)||
       signBoundary.Value().motors[0].qr[0]<=0.0f) return 17;
    auto corrupt=k::SerializeDynamicLocalMotorPaletteV1(canonical.Value());
    corrupt[3]^=std::byte{0x80};
    if(k::DeserializeDynamicLocalMotorPaletteV1(1,corrupt)) return 18;
    auto bundle=c::BuildCorpusFoundationBundleV1(); if(!bundle) return 19;
    if(argc==3&&std::string(argv[1])=="--emit") { auto result=b::WriteAllBytes(std::filesystem::path(argv[2]),bundle.Value());if(!result)return 20;std::cout<<b::ToHex(b::Sha256(bundle.Value()))<<'\n';return 0; }
    std::cout<<"Spiral 2 dynamic invocation and independent CPU reference passed: "<<combinations<<" H/F combinations.\n";
    return combinations==108?0:21;
}
