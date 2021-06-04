import styled from 'styled-components'

export const BubbleContainer = styled.div`
  display: flex;
  width: 100%;
  flex-direction: column;
  align-items: center;
  justify-content: space-between;
  border-radius: 12px;
  padding: 5px 12px;
  background-color: ${(p) => p.theme.color.background02};
  border: ${(p) => `1px solid ${p.theme.color.divider01}`};
  margin-bottom: 12px;
`

export const SelectWrapper = styled.div`
  display: flex;
  width: 100%;
  flex-direction: column;
  align-items: center;
  justify-content: flex-start;
`

export const SelectScrollSearchContainer = styled.div`
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: flex-start;
  overflow-y: scroll;
  overflow-x: hidden;
  position: absolute;
  top: 96px;
  bottom: 18px;
  left: 18px;
  right: 18px;
`

export const SelectScrollContainer = styled.div`
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: flex-start;
  overflow-y: scroll;
  overflow-x: hidden;
  position: absolute;
  top: 50px;
  bottom: 18px;
  left: 18px;
  right: 18px;
`