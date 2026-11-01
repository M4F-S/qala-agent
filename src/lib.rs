//! QALA Autonomous Enterprise OS
//! Target: Best Business AI Agent | Dubai AI Agents Championship 2026-2027

pub mod clearance;
pub mod mesh;
pub mod safety;

/// Direct regulatory access module
pub mod regulatory {
    pub use crate::mesh::hr as labor;
    pub use crate::mesh::trade;
}

pub use clearance::{
    run_clearance_workflow, ContainerClearanceRequest, ContainerClearanceResult,
};
pub use mesh::{
    AgencyMesh, Department, MeshTask, SpecialistResponse, SpecialistVerdict,
    FinanceSpecialist, HrSpecialist, LegalSpecialist, ManagingDirector,
    OperationsSpecialist, ProcurementSpecialist, TradeSpecialist,
};
pub use safety::{ApprovalSlot, PolicyResult, QalaVerdict, SafetyGovernor, ToolCallContext};
